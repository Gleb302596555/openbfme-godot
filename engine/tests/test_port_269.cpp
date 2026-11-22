// OpenBFME unit tests. GPL-3.0.
// Port 269 (issue #636 / P-154): the One Ring dropped on the ground is not on the radar.
//
// RotWK 2.01 (workspace/RotWK/game.dat), read linearly:
//   * Object::getRadarPriority RW 0x68EBE9..0x68EC24: movsx edi, byte [template+0x600];
//     when INVALID it becomes STRUCTURE only for a garrisonable contain (Object+0x258, vslot 0x10,
//     RW 0x68EC05..0x68EC10) or CAPTURABLE (template+0x10E & 2, RW 0x68EC14).
//   * Radar::addObject RW 0x6D9042: isPriorityVisible RW 0x6D8B21 rejects 0/1 (RW 0x6D9068..0x6D906F);
//     the rest of the function (structure / wall gates) has no CRATE, ONE_RING or REVEAL_TO_ALL test.
//   * The minimap mark is a radar EVENT, not an object blip: ClientUpdate = RadarMarkerClientUpdate
//     (registry RW 0x65287A, MarkerType field RW 0x8CE0CA) with MarkerType=PingOneRing / PingBeacon.
//     clientUpdate RW 0x8CE116 (slot +0x30 of the client-update vtable RW 0xC759E8, entry
//     RW 0xC75A18) asks TheRadar for the marker (W3DRadar::createMarker, vslot +0x30 / RW 0x44F0FA),
//     stores it at RW 0x8CDFC9 and moves it to the drawable (RW 0x676711 -> RW 0x9F9E7B);
//     W3DRadar::drawEvents RW 0x44DE58 (marker loop RW 0x44EDAE) paints the movie ping.
//   * There is no KindOf rule: REVEAL_TO_ALL (template+0x112 & 0x80) is read only by the shroud /
//     vision look mask RW 0x694ADA (0xFFFFF, already ported as S-564); ONE_RING (byte +0x122 & 8)
//     has no byte test at all and the one dword test RW 0x97CE0F (mask 0x82000) is an AI object scan;
//     a full .text sweep found no other template +0x600 writer than the default (RW 0x73FFD2), the
//     INI parser (RW 0x42E976) and the template copy (RW 0x6D259B / 0x6D25A1).
// The port's Godot minimap is Radar::renderOverlay -> renderObjects (RADAR-1); DrawableModule has no
// clientUpdate hook, so no RadarMarker is ever created. PORT-269 carries the retail marker rule into
// that existing path: a template with a RadarMarkerClientUpdate/MarkerType and an INVALID
// RadarPriority is listed with the RotWK priority (INVALID). No template name and no KindOf bit.
#include "doctest.h"

#include "ProdTestUtil.h"

#include "GameClient/LogicSnapshot.h"
#include "GameClient/Radar.h"
#include "GameClient/RadarMarkerClientUpdate.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/System/ShroudManager.h"
#include "GameLogic/WeaponStores.h"

#include <cstdint>
#include <vector>

using namespace prodtest;

namespace
{
// Ennorath 1.9.8.3 crate.ini TheDroppedRing / TheDroppedElfRing / TheDroppedElfRingBlue and the
// PalantirShard* crates: no RadarPriority field (default/object.ini:159 = INVALID). TheDroppedRing
// carries the PingOneRing marker at crate.ini:1095; the shards carry PingBeacon; TheRingNenya /
// TheRingVilya carry the same marker without ONE_RING; object/projectile.ini DieJagdLurtzPing is a
// non-crate PingBeacon. All are reproduced synthetically here, no game files.
const char kPort269Objects[] =
	"Object P269Ring\n"
	"  KindOf = SELECTABLE IMMOBILE NOT_AUTOACQUIRABLE UNATTACKABLE REVEAL_TO_ALL CRATE ONE_RING\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  ClientUpdate = RadarMarkerClientUpdate ModuleTag_RadarMarker\n"
	"    MarkerType = PingOneRing\n"
	"  End\n"
	"End\n"
	"Object P269ElfRing\n" // TheRingNenya / TheRingVilya: no ONE_RING bit, still a PingOneRing marker
	"  KindOf = SELECTABLE IMMOBILE NOT_AUTOACQUIRABLE UNATTACKABLE REVEAL_TO_ALL CRATE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  ClientUpdate = RadarMarkerClientUpdate ModuleTag_RadarMarker\n"
	"    MarkerType = PingOneRing\n"
	"  End\n"
	"End\n"
	"Object P269Shard\n" // PalantirShard / PalantirShard01..07: PingBeacon
	"  KindOf = SELECTABLE IMMOBILE NOT_AUTOACQUIRABLE UNATTACKABLE REVEAL_TO_ALL CRATE ONE_RING\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  ClientUpdate = RadarMarkerClientUpdate ModuleTag_RadarMarker\n"
	"    MarkerType = PingBeacon\n"
	"  End\n"
	"End\n"
	"Object P269ProjectilePing\n" // DieJagdLurtzPing: not a crate, not ONE_RING, not REVEAL_TO_ALL
	"  KindOf = NO_COLLIDE IMMOBILE UNATTACKABLE IGNORE_FOR_VICTORY IGNORE_FOR_EVA_SPEECH_POSITION MOVE_ONLY\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  ClientUpdate = RadarMarkerClientUpdate ModuleTag_RadarMarker\n"
	"    MarkerType = PingBeacon\n"
	"  End\n"
	"End\n"
	"Object P269Prop\n" // a plain IMMOBILE prop: none of the ring's kinds and no marker module
	"  KindOf = IMMOBILE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"End\n"
	"Object P269NoType\n" // no real Ennorath template leaves MarkerType empty; the port's module then creates no marker
	"  KindOf = SELECTABLE IMMOBILE NOT_AUTOACQUIRABLE UNATTACKABLE REVEAL_TO_ALL CRATE ONE_RING\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  ClientUpdate = RadarMarkerClientUpdate ModuleTag_RadarMarker\n"
	"  End\n"
	"End\n"
	"Object P269NotOnRadar\n" // an explicit NOT_ON_RADAR stays off: the marker does not override it
	"  KindOf = SELECTABLE IMMOBILE NOT_AUTOACQUIRABLE UNATTACKABLE REVEAL_TO_ALL CRATE ONE_RING\n"
	"  RadarPriority = NOT_ON_RADAR\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  ClientUpdate = RadarMarkerClientUpdate ModuleTag_RadarMarker\n"
	"    MarkerType = PingOneRing\n"
	"  End\n"
	"End\n"
	"Object P269Unit\n" // a normal RadarPriority = UNIT: unchanged, no marker module
	"  KindOf = SELECTABLE IMMOBILE\n"
	"  RadarPriority = UNIT\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"End\n";

struct Port269World : ProdWorld
{
	WeaponStores stores;
	Port269World()
	{
		stores.install();
		load(kPort269Objects);
		logic->settings().bodyThresholdsLoaded = true;
		local = alice();
	}
	Player *local = nullptr;

	ObjectSnapshot::RadarEntry capture(Object &o)
	{
		ObjectSnapshot::RadarEntry e;
		Radar::captureObject(*logic, o, local, e);
		return e;
	}

	ObjectSnapshot::RadarEntry capture(const char *name)
	{
		Object *o = make(name, teamOf("Alice"));
		REQUIRE(o != nullptr);
		return capture(*o);
	}
};

ObjectSnapshot entry(ObjectID id, float x, float y, const ObjectSnapshot::RadarEntry &radar)
{
	ObjectSnapshot o;
	o.id = id;
	o.position = { x, y, 0.0f };
	o.radar = radar;
	o.radarBlip = radar.listed;
	return o;
}

std::size_t lit(const std::vector<std::uint32_t> &t)
{
	std::size_t n = 0;
	for (std::uint32_t c : t)
	{
		n += c != 0;
	}
	return n;
}
} // namespace

TEST_CASE("PORT-269 dropped ring: a RadarMarkerClientUpdate marker lists an INVALID-priority object with the RotWK priority")
{
	Port269World w;
	Object *ringObj = w.make("P269Ring", w.teamOf("Alice"));
	Object *propObj = w.make("P269Prop", w.teamOf("Alice"));
	REQUIRE(ringObj != nullptr);
	REQUIRE(propObj != nullptr);
	const ObjectSnapshot::RadarEntry ring = w.capture(*ringObj);
	const ObjectSnapshot::RadarEntry prop = w.capture(*propObj);

	// main 958dc70 (checked before the change): both were unlisted and both priorities INVALID.
	REQUIRE(ring.listed);
	CHECK(ring.priority == Radar::RADAR_PRIORITY_INVALID); // RW 0x68EBE9: the ring has no RadarPriority
	CHECK(ring.shape == Radar::SHAPE_DOT);
	const bool hasColor = ring.color != 0u;
	CHECK(hasColor);
	CHECK_FALSE(prop.listed);
	CHECK(prop.priority == Radar::RADAR_PRIORITY_INVALID);

	// The real LogicSnapshot path (the one GodotInGameHud presents) must give the ring a blip too:
	// its radarBlip / shroud gates do not hide the SELECTABLE crate, and captureObject lists it.
	const std::shared_ptr<const LogicSnapshot> built = LogicSnapshot::build(*w.logic, nullptr, false, 0);
	const ObjectSnapshot *ringRec = built->find(ringObj->getID());
	const ObjectSnapshot *propRec = built->find(propObj->getID());
	REQUIRE(ringRec != nullptr);
	REQUIRE(propRec != nullptr);
	CHECK(ringRec->radarBlip);
	CHECK(ringRec->radar.listed);
	CHECK(ringRec->radar.priority == Radar::RADAR_PRIORITY_INVALID);
	CHECK_FALSE(ringRec->shroudedForLocal);
	const bool notFogged = ringRec->objectShroud == OBJECTSHROUD_INVALID
		|| ringRec->objectShroud == OBJECTSHROUD_CLEAR
		|| ringRec->objectShroud == OBJECTSHROUD_PARTIAL_CLEAR;
	CHECK(notFogged);
	CHECK_FALSE(propRec->radar.listed);

	// The port's Godot radar path (Radar::renderOverlay -> renderObjects) now paints the ring's
	// 2 x 2 dot and still paints nothing for the plain IMMOBILE prop.
	LogicSnapshot snap;
	snap.objects.push_back(entry(1, 640.0f, 640.0f, ring));
	snap.objects.push_back(entry(2, 100.0f, 100.0f, prop));
	std::vector<std::uint32_t> texels;
	Radar::renderObjects(snap, 1280.0f, 1280.0f, 0, 0xFFFFFFFFu, texels);
	REQUIRE(texels.size() == (std::size_t)Radar::kCells * Radar::kCells);
	CHECK(lit(texels) == 4);

	LogicSnapshot propOnly;
	propOnly.objects.push_back(entry(2, 100.0f, 100.0f, prop));
	Radar::renderObjects(propOnly, 1280.0f, 1280.0f, 0, 0xFFFFFFFFu, texels);
	CHECK(lit(texels) == 0);
}

TEST_CASE("PORT-269 neighbours: every other owner of the marker rule, and the objects it leaves alone")
{
	Port269World w;
	// The same rule adds all the other templates with a RadarMarkerClientUpdate and no RadarPriority:
	// the dropped elf rings, PalantirShard*, TheRingNenya / TheRingVilya (no ONE_RING bit) and the
	// non-crate DieJagdLurtzPing projectile. It is neither a template name nor a KindOf test.
	for (const char *name : { "P269ElfRing", "P269Shard", "P269ProjectilePing" })
	{
		CAPTURE(name);
		const ObjectSnapshot::RadarEntry e = w.capture(name);
		CHECK(e.listed);
		CHECK(e.priority == Radar::RADAR_PRIORITY_INVALID);
	}
	// An empty MarkerType, an explicit NOT_ON_RADAR and a plain prop are not added; a normal UNIT
	// keeps its old listed state and priority. Nothing is removed from the radar.
	const ObjectSnapshot::RadarEntry noType = w.capture("P269NoType");
	const ObjectSnapshot::RadarEntry off = w.capture("P269NotOnRadar");
	const ObjectSnapshot::RadarEntry unit = w.capture("P269Unit");
	const ObjectSnapshot::RadarEntry prop = w.capture("P269Prop");
	CHECK_FALSE(noType.listed);
	CHECK(noType.priority == Radar::RADAR_PRIORITY_INVALID);
	CHECK_FALSE(off.listed);
	CHECK(off.priority == Radar::RADAR_PRIORITY_NOT_ON_RADAR);
	CHECK(unit.listed);
	CHECK(unit.priority == Radar::RADAR_PRIORITY_UNIT);
	CHECK_FALSE(prop.listed);
}

TEST_CASE("PORT-269 construction: MarkerType is an INI field of the client update module, not a template name")
{
	Port269World w;
	for (const char *name : { "P269Ring", "P269Shard", "P269ElfRing" })
	{
		CAPTURE(name);
		const ThingTemplate *tt = w.logic->things().findTemplate(name);
		REQUIRE(tt != nullptr);
		bool found = false;
		for (const ThingTemplate::Nugget &n : tt->clientUpdateModules().nuggets())
		{
			if (n.name != "RadarMarkerClientUpdate")
			{
				continue;
			}
			found = true;
			const auto *data = dynamic_cast<const RadarMarkerClientUpdateModuleData *>(n.data.get());
			REQUIRE(data != nullptr);
			CHECK_FALSE(data->m_markerType.empty());
		}
		CHECK(found);
	}
	// The object without the field: the port's RadarMarkerClientUpdate::clientUpdate returns on an
	// empty MarkerType, so the port creates no marker and this rule adds no blip. RW 0x8CE116 instead
	// passes the empty name RW 0xBD0C3F to createMarker; no Ennorath owner of the module has this case.
	const ThingTemplate *tt = w.logic->things().findTemplate("P269NoType");
	REQUIRE(tt != nullptr);
	bool found = false;
	for (const ThingTemplate::Nugget &n : tt->clientUpdateModules().nuggets())
	{
		if (n.name == "RadarMarkerClientUpdate")
		{
			found = true;
			const auto *data = dynamic_cast<const RadarMarkerClientUpdateModuleData *>(n.data.get());
			REQUIRE(data != nullptr);
			CHECK(data->m_markerType.empty());
		}
	}
	CHECK(found);
}
