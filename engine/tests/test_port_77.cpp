// OpenBFME. GPL-3.0.
//
// PORT-77 (issue #209): W3DSupplyDraw::SupplyBonePrefix. The RotWK reader RW 0x4CA784 gets the drawable-wide
// pristine SUP count (RW 0x672A73), computes ceil(current boxes / StartingBoxes * count) (RW 0x4CA7E1..0x4CA805),
// and hides/shows only the changed numbered sub-objects (RW 0x4CA80D..0x4CA8AD, visibility vcall RW 0x4C3B25).
// These tests use a five-bone pile assembled from in-memory W3D chunks; they need no game or mod files.

#include "doctest.h"

#include "Common/Thing/ThingTemplate.h"
#include "GameClient/ClientEvents.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Module/DockUpdate.h"
#include "GameLogic/Module/SupplyTruckAIUpdate.h"
#include "MoveTestUtil.h"
#include "GameClient/DrawableLaunchBones.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DModelDraw.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DModelDrawVariants.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DScriptedModelDraw.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DSupplyDraw.h"
#include "W3DDrawTestUtil.h"
#include "W3dSynth.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace drawtest;
using namespace w3dsynth;

namespace
{
const char *kSupplyModelState =
	"DefaultModelConditionState\n"
	"  Model = SUPPLY_SKN\n"
	"  Skeleton = SUPPLY_SKL\n"
	"End\n"
	"ExtraPublicBone = SUP\n"
	"End\n";

void makeSupplyAssets(MemoryFileSource &fs)
{
	std::vector<W3dPivotStruct> pivots;
	pivots.push_back(pivot("ROOTTRANSFORM", 0xFFFFFFFFu, 0.0f, 0.0f, 0.0f));
	for (int i = 1; i <= 5; ++i)
	{
		char name[16];
		std::snprintf(name, sizeof(name), "SUP%02d", i);
		pivots.push_back(pivot(name, 0, 0.0f, 0.0f, 0.0f));
	}
	fs.Add("art\\w3d\\su\\supply_skl.w3d", hierarchyChunk("SUPPLY_SKL", pivots));

	std::vector<std::uint8_t> model;
	std::vector<SynthSub> subObjects;
	for (int i = 1; i <= 5; ++i)
	{
		char name[16];
		std::snprintf(name, sizeof(name), "SUP%02d", i);
		SynthMesh mesh;
		mesh.Name = name;
		mesh.Container = "SUPPLY_SKN";
		mesh.Vertices = { { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } };
		mesh.Triangles = { 0, 1, 2 };
		append(model, meshChunk(mesh));
		subObjects.push_back({ std::string("SUPPLY_SKN.") + name, (std::uint32_t)i });
	}
	SynthMesh other;
	other.Name = "OTHER";
	other.Container = "SUPPLY_SKN";
	other.Vertices = { { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } };
	other.Triangles = { 0, 1, 2 };
	append(model, meshChunk(other));
	subObjects.push_back({ "SUPPLY_SKN.OTHER", 0 });
	append(model, hlodChunk("SUPPLY_SKN", "SUPPLY_SKL", subObjects));
	fs.Add("art\\w3d\\su\\supply_skn.w3d", model);
}

struct Port77Rig
{
	MemoryFileSource files;
	std::unique_ptr<WW3DAssetManager> assetManager;
	std::unique_ptr<WW3DDrawAssets> drawAssets;
	W3DClientRandom random{ RandomAlgorithm::ZH_CarryChain };
	Harness harness;
	std::shared_ptr<W3DSupplyDrawModuleData> supplyData;
	ThingTemplate thingTemplate;
	std::unique_ptr<W3DScriptedModelDraw> draw;
	std::unique_ptr<W3DScriptedModelDraw> ordinaryDraw;
	std::unique_ptr<W3DSupplyDraw> supply;

	Port77Rig()
	{
		makeSupplyAssets(files);
		assetManager.reset(new WW3DAssetManager(files));
		drawAssets.reset(new WW3DDrawAssets(*assetManager));
		W3DModelDrawModuleData *base = harness.parse(kSupplyModelState);
		if (!base)
		{
			throw std::runtime_error("PORT-77 test fixture could not parse the synthetic model draw: " + harness.error);
		}

		supplyData = std::make_shared<W3DSupplyDrawModuleData>();
		static_cast<W3DModelDrawModuleData &>(*supplyData) = *base;
		supplyData->m_supplyBonePrefix = "SUP";
		supplyData->m_extraPublicBones = { "SUP" }; // supplyrocks.ini: ExtraPublicBone = SUP
		thingTemplate.friend_setTemplateName("Port77SupplyPile");
		thingTemplate.addModuleInfo(thingTemplate.moduleList(MODULETYPE_DRAW), "W3DSupplyDraw", "ModuleTag_01", supplyData, 0, false, false);

		W3DScriptedModelDraw::Options options;
		options.buildBones = false; // the supply query uses the existing pristine-bone provider; no generic bone path is enabled
		draw.reset(new W3DScriptedModelDraw(*supplyData, *drawAssets, random, nullptr, options));
		ordinaryDraw.reset(new W3DScriptedModelDraw(*base, *drawAssets, random, nullptr, options));
		supply.reset(new W3DSupplyDraw(thingTemplate, *supplyData, *drawAssets));
	}

	void setSupply(int current, int starting)
	{
		supply->setSupplyStatus(*draw, ModelConditionFlags(), 1.0f, current, starting);
	}

	size_t visibleSupplySubObjects() const
	{
		const RenderObjPrototype *model = draw->currentModel();
		if (!model)
		{
			return 0;
		}
		size_t visible = 0;
		for (size_t i = 0; i < model->SubObjects.size(); ++i)
		{
			const std::string &name = model->SubObjects[i].Name;
			const size_t dot = name.find('.');
			const std::string shortName = dot == std::string::npos ? name : name.substr(dot + 1);
			if (shortName.size() == 5 && shortName.compare(0, 3, "SUP") == 0 && !draw->isSubObjectHidden((int)i))
			{
				++visible;
			}
		}
		return visible;
	}

	int otherSubObjectIndex() const
	{
		const RenderObjPrototype *model = draw->currentModel();
		return model ? W3DFindSubObjectByName(*model, "OTHER") : -1;
	}
};

struct DetachSupplyHooks
{
	Object &object;
	~DetachSupplyHooks() { object.friend_bindToClient(nullptr); }
};

const char *kDockObjects =
	"Object Port77Collector\n"
	"  KindOf = INFANTRY\n"
	"  Body = ActiveBody BodyTag\n    MaxHealth = 10\n  End\n"
	"  Behavior = WorkerAIUpdate WorkerTag\n    MaxBoxes = 1\n  End\n"
	"End\n"
	"Object Port77SupplySource\n"
	"  KindOf = STRUCTURE\n"
	"  Body = ActiveBody BodyTag\n    MaxHealth = 10\n  End\n"
	"  Behavior = SupplyWarehouseDockUpdate DockTag\n"
	"    NumberApproachPositions = -1\n    StartingBoxes = 2\n  End\n"
	"End\n";
} // namespace

TEST_CASE("PORT-77 SupplyBonePrefix: five synthetic SUP parts show ceil(current / StartingBoxes * 5) at full, half and empty supply (RW 0x4CA784)")
{
	Port77Rig rig;
	REQUIRE(rig.draw->currentModel() != nullptr);

	// Ennorath's supplyrocks.ini uses StartingBoxes = 300 (small rocks: 150); a five-part synthetic model makes
	// the half-supply ceil observable: ceil(150 / 300 * 5) = 3, per RW 0x4CA7E1..0x4CA805.
	rig.setSupply(300, 300);
	CHECK(rig.supply->totalBones() == 5);
	CHECK(rig.supply->visibleBones() == 5);
	CHECK(rig.visibleSupplySubObjects() == 5);

	rig.setSupply(150, 300);
	CHECK(rig.supply->visibleBones() == 3);
	CHECK(rig.visibleSupplySubObjects() == 3);

	rig.setSupply(0, 300);
	CHECK(rig.supply->visibleBones() == 0);
	CHECK(rig.visibleSupplySubObjects() == 0);
}

TEST_CASE("PORT-77 SupplyBonePrefix: only numbered SUP parts change; other sub-objects and the ordinary draw stay untouched")
{
	Port77Rig rig;
	REQUIRE(rig.draw->currentModel() != nullptr);
	const int other = rig.otherSubObjectIndex();
	REQUIRE(other == 5); // five numbered SUP sub-objects precede OTHER in the synthetic HLOD

	rig.setSupply(150, 300);
	CHECK(rig.visibleSupplySubObjects() == 3);
	CHECK_FALSE(rig.draw->isSubObjectHidden(other));
	CHECK(rig.ordinaryDraw->hiddenSubObjects().empty());

	// Increasing the count exercises RW 0x4CA83D..0x4CA83F's show branch (new >= old).
	rig.setSupply(300, 300);
	CHECK(rig.visibleSupplySubObjects() == 5);
	CHECK_FALSE(rig.draw->isSubObjectHidden(other));
	CHECK(rig.ordinaryDraw->hiddenSubObjects().empty());
}

TEST_CASE("PORT-77 SupplyBonePrefix: a warehouse transfer records and applies the drawable supply status; loaded state is replayed without saved draw state (RW 0x8AB623..0x8AB63A, INFERENCE event bridge)")
{
	Port77Rig rig;
	movetest::MoveWorld world(100, 100, kDockObjects);
	Object *collector = world.spawn("Port77Collector", 100.0f, 100.0f);
	Object *source = world.spawn("Port77SupplySource", 100.0f, 100.0f);
	REQUIRE(collector);
	REQUIRE(source);
	auto *dock = dynamic_cast<SupplyWarehouseDockUpdate *>(source->findModule("SupplyWarehouseDockUpdate"));
	REQUIRE(dock);
	REQUIRE(collector->getAIUpdateInterface());
	REQUIRE(collector->getAIUpdateInterface()->getSupplyTruckAIInterface());

	ClientEventRecorder recorder(*world.logic);
	recorder.objectCreated(*source); // queues CREATED, then the initial (2 / 2) supply status
	DetachSupplyHooks detach{ *source };
	std::vector<ClientEvent> created = recorder.take();
	REQUIRE(created.size() == 2);
	CHECK(created[0].kind == ClientEvent::CREATED);
	CHECK(created[1].kind == ClientEvent::SUPPLY_STATUS);
	CHECK(created[1].supplyCurrentBoxes == 2);
	CHECK(created[1].supplyStartingBoxes == 2);
	// The event transport is the existing ordered ClientEvents stream. Route its supply payload to the synthetic
	// drawable helper used by the standalone tests above; no save-side drawable state is involved.
	CHECK(rig.visibleSupplySubObjects() == 5); // a fresh draw starts in the all-visible pose
	rig.setSupply(created[1].supplyCurrentBoxes, created[1].supplyStartingBoxes);
	CHECK(rig.visibleSupplySubObjects() == 5);

	CHECK(dock->action(*collector, nullptr));
	CHECK(dock->boxesStored() == 1);
	std::vector<ClientEvent> gathered = recorder.take();
	REQUIRE(gathered.size() == 1);
	CHECK(gathered[0].kind == ClientEvent::SUPPLY_STATUS);
	CHECK(gathered[0].supplyCurrentBoxes == 1);
	CHECK(gathered[0].supplyStartingBoxes == 2);
	// Deliver the recorded event payload into the same supply draw runtime. The five part model now has three visible SUP bones.
	rig.setSupply(gathered[0].supplyCurrentBoxes, gathered[0].supplyStartingBoxes);
	CHECK(rig.visibleSupplySubObjects() == 3); // ceil(1 / 2 * 5)

	// Loaded-object notification carries the module's restored count; no additional draw-side state is serialized.
	recorder.loadedObject(*source);
	std::vector<ClientEvent> loaded = recorder.take();
	const auto status = std::find_if(loaded.begin(), loaded.end(), [](const ClientEvent &event) { return event.kind == ClientEvent::SUPPLY_STATUS; });
	REQUIRE(status != loaded.end());
	CHECK(status->supplyCurrentBoxes == dock->boxesStored());
	CHECK(status->supplyStartingBoxes == dock->startingBoxes());

	// A fresh drawable starts with no persisted status. Replaying the loaded-object status reconstructs the same look.
	Port77Rig restored;
	REQUIRE(restored.visibleSupplySubObjects() == 5);
	restored.setSupply(status->supplyCurrentBoxes, status->supplyStartingBoxes);
	CHECK(restored.visibleSupplySubObjects() == 3);
}
