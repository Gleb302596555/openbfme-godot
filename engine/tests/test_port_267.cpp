// OpenBFME. GPL-3.0.
// PORT-267 (fork issue 626): the floor record's fade on death — W3DFloorDraw FloorFadeRateOnObjectDeath.
//
// RotWK keeps every drawn floor in one shared W3DFloorBuffer: one record per (drawable, model name) with the record's own opacity (+0x84), the
// fade rate (+0x88) and a visibility state (+0x94 / +0x98). The port keeps that record on the drawable's floor entry
// (DrawEntry::floorFade, GameClient/FloorFade.h). These tests run on synthetic INI only and prove:
//   * RW 0x4CEBCA (the module's doDrawModule) hands the rate over only for an effectively-dead drawable whose data has a non-zero rate, and the
//     record then steps by that rate per client frame (RW 0x4E547D -> RW 0x4E3EC5: opacity + rate, clamped 0 .. 1) down to 0;
//   * a zero rate leaves the opacity the drawable's own (RW 0x4E3F2D..0x4E3FD4), and a live object's floor keeps it (RW 0x4CEBEC..0x4CEBFD);
//   * a hidden floor's record does not step (RW 0x4E3ED3: only state 1) and resumes when it is shown again (RW 0x4E54BE..0x4E54FD);
//   * the record's opacity, rate and state save and load (RW 0x4E504B / RW 0x4E50C3..0x4E50EA), so a save in the middle of the fade resumes at
//     the same opacity;
//   * the neighbour: a prop entry of the same dead object is not touched (the field belongs to W3DFloorDraw's table only, RW 0xBE3548).

#include "doctest.h"
#include "EnnorathScenario.h"
#include "ObjectTestUtil.h"
#include "W3dSynth.h"

#include "Common/ModelState.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameClient/Drawable.h"
#include "GameClient/FloorFade.h"
#include "GameClient/LogicSnapshot.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDrawModules.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DTreeDraw.h"

#include <memory>
#include <string>
#include <vector>

namespace
{
// the synthetic objects: a floor with a rate, a floor without one, a floor whose rate is set but whose floor is hidden while it is built, and a prop
const char *kIni =
	"Object PT267FloorFade\n"
	"  Draw = W3DFloorDraw ModuleTag_DrawFloor\n"
	"    ModelName = PT267_Bib\n"
	"    FloorFadeRateOnObjectDeath = -0.25\n"
	"  End\n"
	"End\n"
	"Object PT267FloorNoRate\n"
	"  Draw = W3DFloorDraw ModuleTag_DrawFloor\n"
	"    ModelName = PT267_Bib\n"
	"  End\n"
	"End\n"
	"Object PT267FloorHidden\n"
	"  Draw = W3DFloorDraw ModuleTag_DrawFloor\n"
	"    ModelName = PT267_Bib\n"
	"    FloorFadeRateOnObjectDeath = -0.5\n"
	"    HideIfModelConditions = AWAITING_CONSTRUCTION\n"
	"  End\n"
	"End\n"
	"Object PT267Prop\n"
	"  Draw = W3DPropDraw ModuleTag_DrawProp\n"
	"    ModelName = PT267_Prop\n"
	"  End\n"
	"End\n";

// everything a Drawable needs, alive for the test (the same rig as the OBJECT tests: a World over synthetic INI, the archives and the draw services)
struct Site
{
	objtest::World w;
	w3dsynth::MemoryFileSource fs;
	WW3DAssetManager assets;
	W3DLuaDrawScriptHost::Options hostOpt;
	DrawServices services;
	std::vector<std::string> problems;

	Site() : assets(fs), services(assets, w.modules, hostOpt) { W3DDrawModules::registerTypedDrawModuleData(w.modules); }

	bool load() { return w.load(kIni).empty(); }

	std::unique_ptr<Drawable> make(const char *name, DrawableID id)
	{
		const ThingTemplate *tt = w.get(name);
		if (!tt)
		{
			return nullptr;
		}
		problems.clear();
		return std::unique_ptr<Drawable>(new Drawable(services, tt, id, (ObjectID)(100 + id), {}, problems));
	}
};

// Every expected opacity below is exact in binary: 1.0 + (-0.25) = 0.75, then 0.5, 0.25, 0.0, and 0.5 + (-0.5) = 0.0, so the checks compare
// the float directly instead of a bound.

// the drawable's only entry (a floor or a prop template above)
DrawEntry &onlyEntry(Drawable &d)
{
	REQUIRE(d.entries().size() == 1);
	return d.entries()[0];
}

// the client hands the record of the object's last frame to the drawable (DrawableManager::syncTransforms -> setSyncedRecord), the port's place
// where the drawable learns the object's status; RW 0x4CEBF6 reads Object + 0x458 bit 0 at each draw module update
void markDead(Drawable &d, bool dead)
{
	static ObjectSnapshot rec; // setSyncedRecord keeps the pointer (the render side asks the same record again), so it outlives this call
	rec = ObjectSnapshot();
	rec.effectivelyDead = dead;
	d.setSyncedRecord(nullptr, &rec);
}
} // namespace

TEST_CASE("PORT-267 a dead floor fades at FloorFadeRateOnObjectDeath (RW 0x4CEBCA -> RW 0x46AFCA -> RW 0x4E5A98, RW 0x4E3EC5)")
{
	Site site;
	REQUIRE(site.load());
	std::unique_ptr<Drawable> d = site.make("PT267FloorFade", 1);
	REQUIRE(d != nullptr);
	DrawEntry &e = onlyEntry(*d);
	REQUIRE(e.kind == W3D_DRAWKIND_FLOOR);
	CHECK(e.floorFade.opacity() == 1.0f); // INFERENCE: a record starts at 1.0
	CHECK(e.floorFade.rate() == 0.0f);

	// alive: RW 0x4CEBEC..0x4CEBFD does not pass, so the record never takes the rate and keeps the drawable's opacity (RW 0x4E3F2D..0x4E3FD4)
	d->updateFloorFades();
	CHECK(e.floorFade.state() == FloorFade::Record::STATE_VISIBLE);
	CHECK(e.floorFade.rate() == 0.0f);
	CHECK(e.floorFade.opacity() == 1.0f);

	// dead: the rate goes to the record, and the same pass steps the opacity by it (RW 0x4E3F00..0x4E3F08)
	markDead(*d, true);
	d->updateFloorFades();
	CHECK(e.floorFade.rate() == -0.25f);
	CHECK(e.floorFade.opacity() == 0.75f);
	d->updateFloorFades();
	CHECK(e.floorFade.opacity() == 0.5f);
	d->updateFloorFades();
	CHECK(e.floorFade.opacity() == 0.25f);
	d->updateFloorFades();
	CHECK(e.floorFade.opacity() == 0.0f);

	// the clamp at 0 (RW 0x4E3F0C..0x4E3F11): the record stays at 0 and the state stays visible
	for (int i = 0; i < 5; ++i)
	{
		d->updateFloorFades();
	}
	CHECK(e.floorFade.opacity() == 0.0f);
	CHECK(e.floorFade.state() == FloorFade::Record::STATE_VISIBLE);
}

TEST_CASE("PORT-267 a floor without a rate keeps the drawable's opacity (RW 0x4E3F2D..0x4E3FD4), and a prop entry is not touched")
{
	Site site;
	REQUIRE(site.load());

	std::unique_ptr<Drawable> floor = site.make("PT267FloorNoRate", 2);
	REQUIRE(floor != nullptr);
	DrawEntry &f = onlyEntry(*floor);
	REQUIRE(f.kind == W3D_DRAWKIND_FLOOR);
	markDead(*floor, true);
	floor->setDrawableOpacity(0.4f); // the drawable's own opacity (+0xB0)
	floor->updateFloorFades();
	CHECK(f.floorFade.rate() == 0.0f);
	CHECK(f.floorFade.opacity() == 0.4f);
	floor->setDrawableOpacity(0.25f);
	floor->updateFloorFades();
	CHECK(f.floorFade.opacity() == 0.25f);

	// the neighbour: W3DPropDraw's table (RW 0xBE3340) has no FloorFadeRateOnObjectDeath, so even a dead object's prop keeps its opacity
	std::unique_ptr<Drawable> prop = site.make("PT267Prop", 3);
	REQUIRE(prop != nullptr);
	DrawEntry &p = onlyEntry(*prop);
	REQUIRE(p.kind == W3D_DRAWKIND_PROP);
	markDead(*prop, true);
	for (int i = 0; i < 4; ++i)
	{
		prop->updateFloorFades();
	}
	CHECK(p.floorFade.rate() == 0.0f);
	CHECK(p.floorFade.opacity() == 1.0f);
}

TEST_CASE("PORT-267 a hidden floor's record does not step and resumes when shown (RW 0x4E3ED3, RW 0x4E54BE..0x4E54FD)")
{
	Site site;
	REQUIRE(site.load());
	std::unique_ptr<Drawable> d = site.make("PT267FloorHidden", 4);
	REQUIRE(d != nullptr);
	DrawEntry &e = onlyEntry(*d);
	REQUIRE(e.kind == W3D_DRAWKIND_FLOOR);

	markDead(*d, true);
	d->updateFloorFades();
	CHECK(e.floorFade.state() == FloorFade::Record::STATE_VISIBLE);
	CHECK(e.floorFade.rate() == -0.5f);
	CHECK(e.floorFade.opacity() == 0.5f);

	// HideIfModelConditions matches (RENDER-2): the floor is not drawn, so RW 0x4E42DE's visibility fails and the state becomes 2 (RW 0x4E54DD)
	ModelConditionFlags underConstruction;
	underConstruction.set(ModelCondition::indexOf("AWAITING_CONSTRUCTION"));
	d->setModelConditionFlags(underConstruction);
	REQUIRE(e.conditionHidden);
	d->updateFloorFades();
	CHECK(e.floorFade.state() == FloorFade::Record::STATE_HIDDEN);
	CHECK(e.floorFade.opacity() == 0.5f); // frozen while hidden
	d->updateFloorFades();
	CHECK(e.floorFade.opacity() == 0.5f);

	// shown again: state 2 becomes 1 (RW 0x4E54F2) and the fade continues
	d->setModelConditionFlags(ModelConditionFlags());
	REQUIRE_FALSE(e.conditionHidden);
	d->updateFloorFades();
	CHECK(e.floorFade.state() == FloorFade::Record::STATE_VISIBLE);
	CHECK(e.floorFade.opacity() == 0.0f);
}

TEST_CASE("PORT-267 a record saved in the middle of the fade loads back at the same opacity (RW 0x4E504B, RW 0x4E50C3..0x4E50EA)")
{
	Site site;
	REQUIRE(site.load());
	std::unique_ptr<Drawable> d = site.make("PT267FloorFade", 5);
	REQUIRE(d != nullptr);
	DrawEntry &e = onlyEntry(*d);
	markDead(*d, true);
	d->updateFloorFades();
	d->updateFloorFades();
	REQUIRE(e.floorFade.opacity() == 0.5f);

	XferSave save;
	e.floorFade.xfer(save);
	CHECK(save.data().size() == 12); // the two floats (+0x84, +0x88) and one four-byte integer (+0x98), as RW 0x4E50C3 / 0x4E50D1 / 0x4E50E1

	FloorFade::Record loaded;
	XferLoad load(save.data());
	loaded.xfer(load);
	CHECK(loaded.opacity() == 0.5f);
	CHECK(loaded.rate() == -0.25f);
	CHECK(loaded.state() == FloorFade::Record::STATE_VISIBLE);

	// the loaded record resumes the fade at the same place
	loaded.step(1.0f);
	CHECK(loaded.opacity() == 0.25f);
	loaded.step(1.0f);
	CHECK(loaded.opacity() == 0.0f);
}
