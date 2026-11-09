// OpenBFME. GPL-3.0.
// PORT-79 (issue #211): the ObjectCreationList CreateObject nugget's ParticleSystem (+ 0x24), InheritAttributesFromSource (+ 0x92) and PreserveLayer (+ 0x9D).
// The RW addresses the port follows (read in the OCL executor RW 0x5F0EE6 / the per-object part RW 0x5F1B5A; see ObjectCreationList.h):
//   * ParticleSystem RW 0x5F1DC5: the nugget names the system at + 0x24 (table RW 0xBF6E50 row `ParticleSystem`, parsed by RW 0x42ED19's string form); the
//     executor makes it with createParticleSystem(template, createSlaves = 1) (RW 0x5F526A) and attaches it to the object it just made (RW 0x5F32D7).
//     An unknown name is NULL in the FXParticleSystem store (RW 0x73AECB) and makes nothing. The handle is dropped (BFME2 1.06
//     GenericObjectCreationNugget::doStuffToObj: `createParticleSystem(template, true)->attachToObject(obj)`, the result unused), so nothing keeps its ID.
//   * InheritAttributesFromSource RW 0x5F13C8 (field + 0x92, table RW 0xBF74F8): the made object's drawable (RW 0x70E013) is flushed with
//     RW 0x67449C(immediate = 1) — every draw module takes the object's model conditions at once, whether or not the drawable is dirty (RW 0x6744A3's
//     early return is what the 0 argument takes).
//   * PreserveLayer RW 0x5F1412 (field + 0x9D): with a source and no container, the source's layer (RW 0x68BBE0, Object + 0x428 through its AI) when it is
//     not the ground is put on the made object (BFME2 1.06: `if (layer != LAYER_GROUND) debris->setLayer(layer)`).
// Synthetic data only: no retail file is read, so the tests pass without the game and the mod.
#include "doctest.h"

#include "GameClient/ClientEvents.h"
#include "CombatTestUtil.h"
#include "Common/Thing/ThingFactory.h"
#include "GameClient/FXPlayback.h"
#include "GameClient/LiveFX.h"
#include "GameClient/ParticleSys.h"
#include "GameLogic/FXEvents.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "FxTestUtil.h"
#include "LogicTestUtil.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace combattest;

namespace
{
// one FXParticleSystem template: a one-shot system that draws nothing (the store only has to resolve the name, as RW 0x73AECB does)
const char kParticles[] =
	"FXParticleSystem Port79Fog\n"
	"  System\n"
	"    Priority = ALWAYS_RENDER\n"
	"    IsOneShot = Yes\n"
	"    Shader = ALPHA\n"
	"    Type = PARTICLE\n"
	"    ParticleName = EXfog.tga\n"
	"    Lifetime = 30 30\n"
	"    SystemLifetime = 30\n"
	"    Size = 4 4\n"
	"  End\n"
	"End\n";

// the object the nuggets make: an AI update (a pathfind adapter, the layer's holder) and a body/geometry like the other combat tests
const char kMade[] =
	"Object Port79Made\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = Walkers\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"  End\n"
	"End\n";

// the world of the OCL tests: the combat fixture (a 100 x 100 map, the two players), the OCL store and the FXParticleSystem store
struct Port79World : CombatWorld
{
	ObjectCreationListStore store{ w.keys };
	ObjectCreationListStore *saved = TheObjectCreationListStore;
	initest::Fixture fx;
	FXParticleSystem::FXParticleSystemTemplateStore particles;
	fxtest::TestEnv fxEnv; // the particle manager's environment (FxTestUtil.h: flat ground, a logging client random)

	explicit Port79World(const char *lists)
		: CombatWorld(kMade)
	{
		TheObjectCreationListStore = &store;
		w.fx.env.blocks.registerBlock("ObjectCreationList", [](INI *ini) { ObjectCreationListStore::parseObjectCreationListDefinitionGlobal(ini); });
		const std::string err = w.load(lists, INI_LOAD_OVERWRITE, "objectcreationlist.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
	~Port79World() { TheObjectCreationListStore = saved; }

	const ObjectCreationList &list(const char *name)
	{
		const ObjectCreationList *l = store.findObjectCreationList(name);
		REQUIRE_MESSAGE(l, name);
		return *l;
	}
	std::vector<FXEvent> events() const { return logic->fxEvents().frameEvents(); }
	// the events of one kind, copied (the log's own vector is rebuilt every frame)
	std::vector<FXEvent> byKind(FXEvent::Kind kind) const
	{
		std::vector<FXEvent> out;
		for (const FXEvent &e : events())
		{
			if (e.kind == kind)
			{
				out.push_back(e);
			}
		}
		return out;
	}
	// the FXParticleSystem store the events resolve in, loaded from kParticles
	void loadParticles()
	{
		fx.env.blocks.registerBlock("FXParticleSystem", [this](INI *ini) {
			FXParticleSystem::FXParticleSystemTemplateStore *savedStore = FXParticleSystem::TheFXParticleSystemTemplateStore;
			FXParticleSystem::TheFXParticleSystemTemplateStore = &particles;
			FXParticleSystem::ParseFXParticleSystemDefinitionGlobal(ini);
			FXParticleSystem::TheFXParticleSystemTemplateStore = savedStore;
		});
		const std::string err = initest::loadError(fx.env, "fxp.ini", kParticles);
		REQUIRE_MESSAGE(err.empty(), err);
	}
};

// the client side the InheritAttributesFromSource flush is observed on: the recorder's events, and a hook that counts the forced flushes
struct FlushRecorder : ObjectClientHooks
{
	std::vector<bool> immediate;
	// the recorder binds itself to every object it is told about (ClientEventRecorder::objectCreated, Object::friend_bindToClient)
	void objectCreated(Object &obj) override { obj.friend_bindToClient(this); }
	void objectDestroyed(Object &obj) override { obj.friend_bindToClient(nullptr); }
	void flushModelConditions(Object &, bool forced) override { immediate.push_back(forced); }
};

const char kLists[] =
	"ObjectCreationList Port79WithFog\n"
	"  CreateObject\n"
	"    ObjectNames = Port79Made\n"
	"    ParticleSystem = Port79Fog\n"
	"  End\n"
	"End\n"
	"ObjectCreationList Port79Plain\n"
	"  CreateObject\n"
	"    ObjectNames = Port79Made\n"
	"  End\n"
	"End\n"
	"ObjectCreationList Port79UnknownFog\n"
	"  CreateObject\n"
	"    ObjectNames = Port79Made\n"
	"    ParticleSystem = Port79NoSuchSystem\n"
	"  End\n"
	"End\n"
	"ObjectCreationList Port79Inherit\n"
	"  CreateObject\n"
	"    ObjectNames = Port79Made\n"
	"    InheritAttributesFromSource = Yes\n"
	"  End\n"
	"End\n"
	"ObjectCreationList Port79KeepLayer\n"
	"  CreateObject\n"
	"    ObjectNames = Port79Made\n"
	"  End\n"
	"End\n"
	"ObjectCreationList Port79NoKeepLayer\n"
	"  CreateObject\n"
	"    ObjectNames = Port79Made\n"
	"    PreserveLayer = No\n"
	"  End\n"
	"End\n";
} // namespace

// RW 0x5F1DC5: one CreateObject nugget with ParticleSystem makes one system, attached to the object it made; without the line nothing is made
TEST_CASE("PORT-79 ParticleSystem: the nugget's system is created and attached to the made object (RW 0x5F1DC5), none without the line")
{
	Port79World w(kLists);
	w.loadParticles();
	Object *source = w.unit("Port79Made", 'A', 400, 400);
	REQUIRE(source != nullptr);
	w.logic->fxEvents().beginFrame();

	const std::vector<Object *> made = w.list("Port79WithFog").create(*w.logic, source, *source->getPosition());
	REQUIRE(made.size() == 1);
	const std::vector<FXEvent> attached = w.byKind(FXEvent::OBJECT_PARTICLE_SYSTEM);
	REQUIRE(attached.size() == 1);
	CHECK(*attached[0].fxList == "Port79Fog");
	CHECK(attached[0].primary == made[0]->getID());
	CHECK(std::string(attached[0].site) == "ObjectCreationList");
	// no model-space position is handed over: the attachment places the system at the object's drawable (RW 0x5F32D7 alone)
	CHECK_FALSE(attached[0].hasTransform);

	// the client: the store resolves the template, the system is made and attached to that object
	FXParticleSystem::ParticleSystemManager manager(w.particles, w.fxEnv);
	const FXParticleSystem::ParticleSystemID id = attachCreatedSystem(manager, w.particles, attached[0]);
	REQUIRE(id != FXParticleSystem::INVALID_PARTICLE_SYSTEM_ID);
	const FXParticleSystem::ParticleSystem *sys = manager.findParticleSystemByID(id);
	REQUIRE(sys != nullptr);
	CHECK(sys->getTemplate().getName() == "Port79Fog");
	CHECK(sys->attachedObjectID() == made[0]->getID());

	// without the ParticleSystem line the nugget makes its object and no system
	w.logic->fxEvents().beginFrame();
	const std::vector<Object *> plain = w.list("Port79Plain").create(*w.logic, source, *source->getPosition());
	REQUIRE(plain.size() == 1);
	CHECK(w.byKind(FXEvent::OBJECT_PARTICLE_SYSTEM).empty());
}

// RW 0x73AECB: an unknown template name is NULL in the store — the object is made, no system is
TEST_CASE("PORT-79 ParticleSystem: an unknown template makes the object and no system (RW 0x73AECB), it is reported unresolved")
{
	Port79World w(kLists);
	w.loadParticles();
	Object *source = w.unit("Port79Made", 'A', 420, 400);
	REQUIRE(source != nullptr);
	w.logic->fxEvents().beginFrame();
	const std::vector<Object *> made = w.list("Port79UnknownFog").create(*w.logic, source, *source->getPosition());
	REQUIRE(made.size() == 1);
	const std::vector<FXEvent> attached = w.byKind(FXEvent::OBJECT_PARTICLE_SYSTEM);
	REQUIRE(attached.size() == 1);
	CHECK(*attached[0].fxList == "Port79NoSuchSystem");
	FXParticleSystem::ParticleSystemManager manager(w.particles, w.fxEnv);
	CHECK(attachCreatedSystem(manager, w.particles, attached[0]) == FXParticleSystem::INVALID_PARTICLE_SYSTEM_ID);
	int systems = 0;
	manager.forEachSystem([&](const FXParticleSystem::ParticleSystem &) { ++systems; });
	CHECK(systems == 0);
}

// RW 0x5F13C8: InheritAttributesFromSource flushes the made object's drawable with the immediate flag; without it no forced flush happens
TEST_CASE("PORT-79 InheritAttributesFromSource: the made object's flags reach the draw modules at once (RW 0x67449C(1)), not without the line")
{
	Port79World w(kLists);
	Object *source = w.unit("Port79Made", 'A', 440, 400);
	REQUIRE(source != nullptr);
	FlushRecorder forced;
	w.logic->setClientHooks(&forced);
	struct Unhook
	{
		GameLogic &logic;
		~Unhook() { logic.setClientHooks(nullptr); }
	} unhook{ *w.logic };

	const std::vector<Object *> made = w.list("Port79Inherit").create(*w.logic, source, *source->getPosition());
	REQUIRE(made.size() == 1);
	REQUIRE(forced.immediate.size() == 1);
	CHECK(forced.immediate[0]); // the immediate argument of RW 0x67449C, inside RW 0x5F13C8
	// the newer object of the same list has no forced flush
	(void)w.list("Port79Inherit").create(*w.logic, source, *source->getPosition());
	CHECK(forced.immediate.size() == 2);
	const std::vector<Object *> plain = w.list("Port79Plain").create(*w.logic, source, *source->getPosition());
	REQUIRE(plain.size() == 1);
	CHECK(forced.immediate.size() == 2); // the plain nugget flushes nothing
}

// RW 0x67449C(0) versus RW 0x67449C(1) at the client's recorder: a clean drawable is flushed only with the immediate flag
TEST_CASE("PORT-79 InheritAttributesFromSource: the recorder flushes a clean object only when the call is immediate")
{
	Port79World w(kLists);
	ClientEventRecorder recorder(*w.logic);
	w.logic->setClientHooks(&recorder);
	struct Unhook
	{
		GameLogic &logic;
		~Unhook() { logic.setClientHooks(nullptr); }
	} unhook{ *w.logic };
	Object *object = w.unit("Port79Made", 'A', 460, 400); // created with the recorder bound (ClientEventRecorder::objectCreated)
	REQUIRE(object != nullptr);
	const ObjectID id = object->getID();
	recorder.take();
	// nothing changed on the object: the dirty-only flush is silent (RW 0x6744A3)
	object->flushDrawableModelConditions(false);
	std::vector<ClientEvent> events = recorder.take();
	size_t flush = 0;
	for (const ClientEvent &e : events)
	{
		flush += e.kind == ClientEvent::MODEL_FLUSH && e.object == id ? 1u : 0u;
	}
	CHECK(flush == 0);
	// the immediate flush of RW 0x5F13C8 reaches the draw modules anyway
	object->flushDrawableModelConditions(true);
	events = recorder.take();
	flush = 0;
	bool immediate = false;
	for (const ClientEvent &e : events)
	{
		if (e.kind == ClientEvent::MODEL_FLUSH && e.object == id)
		{
			++flush;
			immediate = e.immediate;
		}
	}
	CHECK(flush == 1);
	CHECK(immediate);
}

// RW 0x5F1412: PreserveLayer puts a source's raised layer on the made object; on the ground nothing changes; PreserveLayer = No keeps the ground
TEST_CASE("PORT-79 PreserveLayer: the made object keeps a source's raised layer, the ground stays the ground, No keeps the ground (RW 0x5F1412)")
{
	Port79World w(kLists);
	Object *source = w.unit("Port79Made", 'A', 480, 400);
	REQUIRE(source != nullptr);
	REQUIRE(source->getAIUpdateInterface() != nullptr);
	const PathfindLayerEnum raised = LAYER_TOP; // 2, the first raised walkway layer (AIPathfindHost.h)
	source->getAIUpdateInterface()->setLayer(raised);
	const std::vector<Object *> kept = w.list("Port79KeepLayer").create(*w.logic, source, *source->getPosition());
	REQUIRE(kept.size() == 1);
	REQUIRE(kept[0]->getAIUpdateInterface() != nullptr);
	CHECK(kept[0]->getAIUpdateInterface()->adapter().getLayer() == raised);
	const std::vector<Object *> notKept = w.list("Port79NoKeepLayer").create(*w.logic, source, *source->getPosition());
	REQUIRE(notKept.size() == 1);
	REQUIRE(notKept[0]->getAIUpdateInterface() != nullptr);
	CHECK(notKept[0]->getAIUpdateInterface()->adapter().getLayer() == LAYER_GROUND);
	// a source on the ground: the made object is on the ground too (nothing is copied)
	source->getAIUpdateInterface()->setLayer(LAYER_GROUND);
	const std::vector<Object *> ground = w.list("Port79KeepLayer").create(*w.logic, source, *source->getPosition());
	REQUIRE(ground.size() == 1);
	REQUIRE(ground[0]->getAIUpdateInterface() != nullptr);
	CHECK(ground[0]->getAIUpdateInterface()->adapter().getLayer() == LAYER_GROUND);
}

// the fields are parsed into the nugget (the tables RW 0xBF6E50 / RW 0xBF74F8)
TEST_CASE("PORT-79 parse: ParticleSystem, InheritAttributesFromSource and PreserveLayer land in the nugget")
{
	Port79World w(kLists);
	const ObjectCreationList &fog = w.list("Port79WithFog");
	REQUIRE(fog.nuggets().size() == 1);
	CHECK(fog.nuggets()[0].particleSystem == "Port79Fog");
	CHECK_FALSE(fog.nuggets()[0].inheritAttributesFromSource);
	CHECK(fog.nuggets()[0].preserveLayer); // the creator's default (RW 0x5F0689): true
	const ObjectCreationList &inherit = w.list("Port79Inherit");
	REQUIRE(inherit.nuggets().size() == 1);
	CHECK(inherit.nuggets()[0].inheritAttributesFromSource);
	const ObjectCreationList &off = w.list("Port79NoKeepLayer");
	REQUIRE(off.nuggets().size() == 1);
	CHECK_FALSE(off.nuggets()[0].preserveLayer);
}
