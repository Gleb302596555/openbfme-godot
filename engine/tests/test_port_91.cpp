// OpenBFME unit tests: PORT-91 (issue #239): W3DModelDraw ShowShadowWhileContained — a contained unit keeps its shadow only while its contain allows it.
//
// TARGET FACTS (RotWK game.dat, checked 2026-11-09; addresses from docs/ennorath/PORT91_RW.md's lane notes):
//   W3DModelDraw::doDrawModule RW 0x4C7848 gates the shadow: a drawable whose object is contained hides its shadow unless the module data's
//   ShowShadowWhileContained (RW 0xBE0FCC, module data + 0x?, parsed since PORT-91's predecessor: W3DModelDraw.cpp's field row) is set.
//   The contain answers through its vslot 0x2C:
//     RW 0x9188EB (`xor al, al; ret`, false) in every contain table of 2.01 but three: HordeContain's RW 0xC5B480, its RotWK subclass's RW 0xC5C110 and
//     AODHordeContain's RW 0xC5D268 carry RW 0x86C737 (the answer of the contain of the horde's container, true while the horde is not contained).
//   The isPassengerAllowedToFire slot 0xB4 is a DIFFERENT question: GarrisonContain / HordeGarrisonContain / TunnelContain answer true there (RW 0x8BD372)
//   and false for slot 0x2C.
// The BFME2 1.06 decomp (Open-BFME-2) has the same recursion idiom (OpenContain::isPassengerAllowedToFire's shape).
//
// The gate in the port lives at the shadow consumption points (GodotGameWorld's instance shadow setters, the static path's per-frame refresh, the
// decal shadow builder), not in castsMap: the module data is at the call site there. The per-frame client paths read the presented snapshot (lane
// COMBAT-4's record carries containedBy; the record's shadowWhileContainedAllowed carries the contain's slot 0x2C answer as the logic computed it).
//
// Not ported here (STOPS.md S-7220): the map-build static layer (GameWorld::build_static_layer) and the map viewer build client-only static instances
// with no logic object to ask, so the gate cannot bind there (a map placement is never contained); W3DTreeDraw / W3DPropDraw / floor draw shadows are not
// model draws and are untouched.
//
// Tests run without game files. The synthetic world offers: a transport (TransportContain, plain), a garrison tower (GarrisonContain: slot 0xB4 true /
// slot 0x2C false), a Swordsman (free rider), a SwordHorde (HordeContain, six members).

#include "doctest.h"
#include "CombatTestUtil.h"
#include "ObjectTestUtil.h"
#include "W3DDrawTestUtil.h"
#include "W3dSynth.h"

#include "GameClient/Drawable.h"
#include "GameClient/LogicSnapshot.h"
#include "GameClient/ObjectShadow.h"
#include "GameClient/SelectionDecals.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DModelDraw.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/Object.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"

#include <memory>
#include <string>
#include <vector>

using namespace combattest;

namespace
{
const char kPort91Objects[] =
	"Object Port91Transport\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 60\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = TransportContain ModuleTag_Contain\n"
	"    Slots = 5\n"
	"  End\n"
	"End\n"
	"Object Port91Garrison\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 60\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = GarrisonContain ModuleTag_Contain\n"
	"    ContainMax = 5\n"
	"    DamagePercentToUnits = 0%\n"
	"  End\n"
	"End\n";

W3DModelDrawModuleData *parseShadowFlag(drawtest::Harness &h, bool value)
{
	const std::string text = std::string("ShowShadowWhileContained = ") + (value ? "Yes" : "No") +
		"\nDefaultModelConditionState\n  Model = ABUNIT_SKN\nEnd\nEnd\n";
	W3DModelDrawModuleData *d = h.parse(text);
	REQUIRE_MESSAGE(d, h.error);
	return d;
}

// a drawable for a combat object (no entries: the synthetic draw modules are not made); the object id is all the decal builder reads of it
struct DecalFixture
{
	objtest::World w;
	w3dsynth::MemoryFileSource fs;
	WW3DAssetManager assets;
	W3DLuaDrawScriptHost::Options hostOpt; // declared before `services`: its initializer reads it
	DrawServices services;
	const ThingTemplate *tt = nullptr;
	std::vector<std::string> problems;

	DecalFixture() : assets(fs), services(assets, w.modules, hostOpt)
	{
		REQUIRE_MESSAGE(w.load(
			"Object ShadowDummy\n"
			"  Shadow = SHADOW_DECAL\n"
			"  ShadowTexture = shadowi\n"
			"  ShadowSizeX = 30\n"
			"  ShadowSizeY = 20\n"
			"End\n").empty(),
			"cannot load ShadowDummy");
		tt = w.get("ShadowDummy");
		REQUIRE(tt != nullptr);
	}
};
} // namespace

TEST_CASE("PORT-91 contain slot 0x2C: a plain contain answers false (RW 0x9188EB), GarrisonContain answers false while its slot 0xB4 answers true, the horde answers its container's answer (RW 0x86C737) or true while uncontained")
{
	CombatWorld w(kPort91Objects);
	Object *transport = w.unit("Port91Transport", 'A', 100, 100);
	Object *tower = w.unit("Port91Garrison", 'A', 300, 100);
	Object *horde = w.unit("SwordHorde", 'A', 500, 100);
	HordeContain *hc = w.hordeOf(horde);
	REQUIRE(hc != nullptr);
	// plain contains: false (RW 0x9188EB), for the contain interface itself and through the default virtual
	CHECK(transport->getContain()->shadowWhileContainedAllowed() == false);
	CHECK(tower->getContain()->shadowWhileContainedAllowed() == false);
	// the slot 0xB4 question is different: GarrisonContain answers true there (RW 0x8BD372) and false for slot 0x2C
	CHECK(tower->getContain()->isPassengerAllowedToFire() == true);
	CHECK(transport->getContain()->isPassengerAllowedToFire() == false);
	// an uncontained horde's slot 0x2C answers true (RW 0x86C737: its container has no contain)
	CHECK(hc->shadowWhileContainedAllowed() == true);
	// the horde enters the transport: the horde's slot 0x2C answers the transport's answer (false)
	REQUIRE(transport->getContain()->addToContain(horde));
	CHECK(horde->getContainedBy() == transport);
	CHECK(hc->shadowWhileContainedAllowed() == false);
	// the horde leaves: true again
	transport->getContain()->removeFromContain(horde);
	CHECK(horde->getContainedBy() == nullptr);
	CHECK(hc->shadowWhileContainedAllowed() == true);
}

TEST_CASE("PORT-91: a contained unit without ShowShadowWhileContained loses its shadow; with it keeps it; uncontained: unchanged (RW 0x4C7848)")
{
	CombatWorld w(kPort91Objects);
	Object *transport = w.unit("Port91Transport", 'A', 100, 100);
	Object *rider = w.unit("Swordsman", 'A', 100, 120);
	Object *free = w.unit("Swordsman", 'B', 700, 700);
	drawtest::Harness hPlain, hShown;
	W3DModelDrawModuleData *plain = parseShadowFlag(hPlain, false);
	W3DModelDrawModuleData *shown = parseShadowFlag(hShown, true);
	CHECK(plain->m_showShadowWhileContained == false);
	CHECK(shown->m_showShadowWhileContained == true);
	// uncontained: the shadow stands either way
	CHECK_FALSE(ObjectShadow::containedShadowSuppressed(free, plain));
	CHECK_FALSE(ObjectShadow::containedShadowSuppressed(free, shown));
	REQUIRE(transport->getContain()->addToContain(rider));
	// contained: without the flag the unit loses its shadow; with it keeps it (RW 0x4C7848's flag branch)
	CHECK(ObjectShadow::containedShadowSuppressed(rider, plain));
	CHECK_FALSE(ObjectShadow::containedShadowSuppressed(rider, shown));
	// a module without the row parsed (default false) loses it too
	W3DModelDrawModuleData defaultData;
	CHECK(defaultData.m_showShadowWhileContained == false);
	CHECK(ObjectShadow::containedShadowSuppressed(rider, &defaultData));
	// a null logic object (unknown): nothing is suppressed
	CHECK_FALSE(ObjectShadow::containedShadowSuppressed((const ::Object *)nullptr, plain));
}

TEST_CASE("PORT-91: a garrisoned unit loses its shadow while its slot 0xB4 allows the rider to fire (RW 0x9188EB vs RW 0x8BD372)")
{
	CombatWorld w(kPort91Objects);
	Object *tower = w.unit("Port91Garrison", 'A', 100, 100);
	Object *soldier = w.unit("Swordsman", 'A', 100, 120);
	drawtest::Harness h;
	W3DModelDrawModuleData *plain = parseShadowFlag(h, false);
	REQUIRE(tower->getContain()->addToContain(soldier));
	CHECK(tower->getContain()->isPassengerAllowedToFire()); // slot 0xB4: garrison true
	CHECK(ObjectShadow::containedShadowSuppressed(soldier, plain)); // slot 0x2C: garrison false
}

TEST_CASE("PORT-91: a contained horde and its members lose their shadows together; an uncontained horde's members keep theirs (RW 0x86C737's recursion)")
{
	CombatWorld w(kPort91Objects);
	Object *transport = w.unit("Port91Transport", 'A', 100, 100);
	Object *horde = w.unit("SwordHorde", 'A', 500, 100);
	const std::vector<Object *> members = w.membersOf(horde);
	REQUIRE(members.size() == 6);
	drawtest::Harness h;
	W3DModelDrawModuleData *plain = parseShadowFlag(h, false);
	// the horde is uncontained: its members are contained by it and keep their shadows
	for (Object *m : members)
	{
		REQUIRE(m->getContainedBy() == horde);
		CHECK_FALSE(ObjectShadow::containedShadowSuppressed(m, plain));
	}
	CHECK_FALSE(ObjectShadow::containedShadowSuppressed(horde, plain));
	// the horde enters the transport: the horde's slot 0x2C answers the transport's false, and so do the members' (the recursion bottoms out there)
	REQUIRE(transport->getContain()->addToContain(horde));
	CHECK(ObjectShadow::containedShadowSuppressed(horde, plain));
	for (Object *m : members)
	{
		CHECK(ObjectShadow::containedShadowSuppressed(m, plain));
	}
	// the horde leaves again: shadows stand
	transport->getContain()->removeFromContain(horde);
	CHECK_FALSE(ObjectShadow::containedShadowSuppressed(horde, plain));
	for (Object *m : members)
	{
		CHECK_FALSE(ObjectShadow::containedShadowSuppressed(m, plain));
	}
}

TEST_CASE("PORT-91: the presented snapshot carries containedBy and the contain's slot 0x2C answer to the client per frame (lane COMBAT-4's record)")
{
	CombatWorld w(kPort91Objects);
	Object *transport = w.unit("Port91Transport", 'A', 100, 100);
	Object *rider = w.unit("Swordsman", 'A', 100, 120);
	Object *horde = w.unit("SwordHorde", 'A', 500, 100);
	const std::vector<Object *> members = w.membersOf(horde);
	REQUIRE(members.size() == 6);
	REQUIRE(transport->getContain()->addToContain(rider));
	const std::shared_ptr<const LogicSnapshot> snap = LogicSnapshot::build(*w.logic, nullptr, false, 0);
	const ObjectSnapshot *riderRec = snap->find(rider->getID());
	REQUIRE(riderRec != nullptr);
	CHECK(riderRec->containedBy == (ObjectID)transport->getID());
	CHECK_FALSE(riderRec->shadowWhileContainedAllowed); // the transport's contain answers false (RW 0x9188EB)
	const ObjectSnapshot *memberRec = snap->find(members[0]->getID());
	REQUIRE(memberRec != nullptr);
	CHECK(memberRec->containedBy == (ObjectID)horde->getID());
	CHECK(memberRec->shadowWhileContainedAllowed); // the horde's slot 0x2C answers true while uncontained (RW 0x86C737)
	// contain the horde in the transport and rebuild: both records now answer the transport's false
	REQUIRE(transport->getContain()->addToContain(horde));
	const std::shared_ptr<const LogicSnapshot> snap2 = LogicSnapshot::build(*w.logic, snap.get(), false, 0);
	const ObjectSnapshot *memberRec2 = snap2->find(members[0]->getID());
	REQUIRE(memberRec2 != nullptr);
	CHECK(memberRec2->containedBy == (ObjectID)horde->getID());
	CHECK_FALSE(memberRec2->shadowWhileContainedAllowed);
	const ObjectSnapshot *hordeRec2 = snap2->find(horde->getID());
	REQUIRE(hordeRec2 != nullptr);
	CHECK(hordeRec2->containedBy == (ObjectID)transport->getID());
	CHECK_FALSE(hordeRec2->shadowWhileContainedAllowed);
	// the snapshot-side gate
	drawtest::Harness hPlain, hShown;
	W3DModelDrawModuleData *plain = parseShadowFlag(hPlain, false);
	CHECK(ObjectShadow::containedShadowSuppressed(memberRec2, plain));
	CHECK(ObjectShadow::containedShadowSuppressed(hordeRec2, plain));
	const ObjectSnapshot *uncontained = snap2->find(members[0]->getID()); // member record: still contained (by the horde), but the answer is false now
	CHECK(uncontained != nullptr);
	W3DModelDrawModuleData *shown = parseShadowFlag(hShown, true); // a second harness: the first one's parse would reset plain
	CHECK_FALSE(ObjectShadow::containedShadowSuppressed(memberRec2, shown)); // the module flag wins
	// the null record (no snapshot): nothing is suppressed
	CHECK_FALSE(ObjectShadow::containedShadowSuppressed((const ObjectSnapshot *)nullptr, plain));
}

TEST_CASE("PORT-91: the record-side gate matches the object-side gate (both bottom out in the same contain answer)")
{
	CombatWorld w(kPort91Objects);
	Object *transport = w.unit("Port91Transport", 'A', 100, 100);
	Object *rider = w.unit("Swordsman", 'A', 100, 120);
	Object *horde = w.unit("SwordHorde", 'A', 500, 100);
	REQUIRE(transport->getContain()->addToContain(rider));
	REQUIRE(transport->getContain()->addToContain(horde));
	drawtest::Harness h;
	W3DModelDrawModuleData *plain = parseShadowFlag(h, false);
	const std::shared_ptr<const LogicSnapshot> snap = LogicSnapshot::build(*w.logic, nullptr, false, 0);
	for (const Object *o : { (const Object *)rider, (const Object *)horde })
	{
		const ObjectSnapshot *rec = snap->find(o->getID());
		REQUIRE(rec != nullptr);
		CHECK(ObjectShadow::containedShadowSuppressed(rec, plain) == ObjectShadow::containedShadowSuppressed(o, plain));
	}
}

TEST_CASE("PORT-91: the decal shadow builder keeps the gate for the decal path (RW 0x4C7848 applies to SHADOW_DECAL too)")
{
	CombatWorld w(kPort91Objects);
	Object *transport = w.unit("Port91Transport", 'A', 100, 100);
	Object *rider = w.unit("Swordsman", 'A', 100, 120);
	Object *free = w.unit("Swordsman", 'B', 700, 700);
	Object *horde = w.unit("SwordHorde", 'A', 500, 100);
	const std::vector<Object *> members = w.membersOf(horde);
	REQUIRE(members.size() == 6);
	DecalFixture f;
	const Coord3D pos{ 100.0f, 200.0f, 10.0f };
	auto fallback = [](float &sx, float &sy) {
		if (sx == 0.0f) sx = 60.0f;
		if (sy == 0.0f) sy = -40.0f;
	};
	Drawable dFree(f.services, f.tt, (DrawableID)1, free->getID(), {}, f.problems);
	dFree.setPosition(&pos);
	SelectionDecal sd;
	REQUIRE(ObjectShadow::buildDrawableDecalShadow(dFree, sd, true, fallback, 0, free)); // uncontained: the decal is made
	REQUIRE(transport->getContain()->addToContain(rider));
	Drawable dContained(f.services, f.tt, (DrawableID)2, rider->getID(), {}, f.problems);
	dContained.setPosition(&pos);
	CHECK_FALSE(ObjectShadow::buildDrawableDecalShadow(dContained, sd, true, fallback, 0, rider)); // contained: no decal shadow
	Drawable dMember(f.services, f.tt, (DrawableID)3, members[0]->getID(), {}, f.problems);
	dMember.setPosition(&pos);
	CHECK(ObjectShadow::buildDrawableDecalShadow(dMember, sd, true, fallback, 0, members[0])); // the horde is uncontained: the member's decal is made
	REQUIRE(transport->getContain()->addToContain(horde));
	CHECK_FALSE(ObjectShadow::buildDrawableDecalShadow(dMember, sd, true, fallback, 0, members[0])); // the horde is contained: the member's decal waits
	Drawable dHorde(f.services, f.tt, (DrawableID)4, horde->getID(), {}, f.problems);
	dHorde.setPosition(&pos);
	CHECK_FALSE(ObjectShadow::buildDrawableDecalShadow(dHorde, sd, true, fallback, 0, horde)); // and the horde's own waits with it
	// without the logic object (not resolved): the decal is made (the gate had nothing to ask)
	CHECK(ObjectShadow::buildDrawableDecalShadow(dContained, sd, true, fallback, 0, nullptr));
}

TEST_CASE("PORT-91: the snapshot's shadowWhileContainedAllowed survives a save/load round trip of the presented record (the flag is rebuilt from the live logic every frame)")
{
	// nothing to serialize: the record is made fresh from the logic each frame (LogicSnapshot::build); what is pinned here is the fill's rule —
	// contained + the containing HordeContain uncontained: allowed; the containing transport: not allowed
	CombatWorld w(kPort91Objects);
	Object *transport = w.unit("Port91Transport", 'A', 100, 100);
	Object *horde = w.unit("SwordHorde", 'A', 500, 100);
	const std::vector<Object *> members = w.membersOf(horde);
	REQUIRE(members.size() == 6);
	const std::shared_ptr<const LogicSnapshot> before = LogicSnapshot::build(*w.logic, nullptr, false, 0);
	const ObjectSnapshot *memberBefore = before->find(members[0]->getID());
	REQUIRE(memberBefore != nullptr);
	CHECK(memberBefore->containedBy == (ObjectID)horde->getID());
	CHECK(memberBefore->shadowWhileContainedAllowed == true);
	REQUIRE(transport->getContain()->addToContain(horde));
	const std::shared_ptr<const LogicSnapshot> after = LogicSnapshot::build(*w.logic, before.get(), false, 0);
	const ObjectSnapshot *memberAfter = after->find(members[0]->getID());
	REQUIRE(memberAfter != nullptr);
	CHECK(memberAfter->shadowWhileContainedAllowed == false);
}
