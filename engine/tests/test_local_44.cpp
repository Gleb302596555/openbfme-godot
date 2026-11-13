// OpenBFME unit tests (Local 44, issue #383): the Uruk pit's castle layout fails to decode
// (RefPack back-reference before the output, RW 0xAA17E0) and the building vanishes when finished
// (P-77, tryInstantUnpack clears the instant-build flag and applies JUST_BUILT even when the
// layout lookup fails, RW 0x79C265). GPL-3.0.
//
// RotWK REF_decode (0xAA17E0) has no bounds check on the back reference: when dist > d - dBegin it
// reads whatever lies before the destination. The caller (0xA9E220) allocates and zeroes the
// declared output range, so retail reads zeros there. The port added a bounds check
// ("back reference before start of output") which broke the Uruk pit layout.
//
// RotWK tryInstantUnpack (0x79C265) clears m_needInstantBuild and sets JUST_BUILT even when
// unpack() fails (layout not found: the code jumps to 0x79becd at the end of the function).
// The port does the same, but a neighbouring valid layout must still load unchanged.

#include "doctest.h"

#include "Libraries/Compression/EAC/refdecode.h"

#include "Common/StateHash.h"
#include "GameLogic/BitFlags.h"
#include "GameLogic/Construction.h"
#include "GameLogic/GameLogicDispatch.h"
#include "GameLogic/Module/CastleModules.h"

#include "ProdTestUtil.h"

#include <string>
#include <vector>

using namespace prodtest;

namespace
{

typedef std::vector<std::uint8_t> Bytes;

Bytes bytes(std::initializer_list<int> v)
{
	Bytes b;
	for (int x : v)
		b.push_back((std::uint8_t)x);
	return b;
}

Bytes cat(Bytes a, const Bytes &b)
{
	a.insert(a.end(), b.begin(), b.end());
	return a;
}

// 10fb header with a 3-byte big-endian decoded size.
Bytes header10(size_t n) { return bytes({ 0x10, 0xFB, (int)(n >> 16) & 0xFF, (int)(n >> 8) & 0xFF, (int)n & 0xFF }); }

bool decode(const Bytes &src, Bytes &out, std::string *err = nullptr)
{
	return REF_decode(src.data(), src.size(), out, nullptr, err);
}

// Synthetic .bse-ish stream: a short-form back reference whose distance (6) is larger than anything
// written yet (0). RW reads zeros before the output; the port must do the same instead of failing.
TEST_CASE("LOCAL-44: RefPack back reference before the start of output reads zeros (RW 0xAA17E0)")
{
	// 0x00 0x05: short form, literal count 0, copy length ((0x00 & 0x1C) >> 2) + 3 = 3,
	// distance ((0x00 & 0x60) << 3) + 5 + 1 = 6 > 0 -> three zeros.
	Bytes src = cat(header10(3), bytes({ 0x00, 0x05, 0xFC }));
	Bytes out;
	std::string err;
	REQUIRE_MESSAGE(decode(src, out, &err), err);
	CHECK(out == Bytes({ 0, 0, 0 }));
	CHECK(err.empty());
}

// A back reference that straddles the start of the output: part before (zeros), part inside.
// "AB" literal, then copy 3 bytes from distance 3: pos -1 (zero), 0 ('A'), 1 ('B') -> "AB\0AB".
TEST_CASE("LOCAL-44: RefPack back reference straddling the output start reads zeros then real bytes (RW 0xAA17E0)")
{
	// 0x02 0x02: short form, literal count 2, copy length 3, distance 3.
	Bytes src = cat(cat(header10(5), bytes({ 0x02, 0x02 })), cat(bytes({ 'A', 'B' }), bytes({ 0xFC })));
	Bytes out;
	std::string err;
	REQUIRE_MESSAGE(decode(src, out, &err), err);
	CHECK(out == Bytes({ 'A', 'B', 0, 'A', 'B' }));
	CHECK(err.empty());
}

} // namespace

namespace
{

const char kInstantUnpackObjects[] =
	"Object Keep\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE COMMANDCENTER\n"
	"  Behavior = CastleMemberBehavior ModuleTag_KeepCM\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_KeepBody\n"
	"    MaxHealth = 1000\n"
	"    MaxHealthDamaged = 500\n"
	"    MaxHealthReallyDamaged = 100\n"
	"  End\n"
	"End\n"
	"Object AlphaCastle\n"
	"  KindOf = IMMOBILE CASTLE_CENTER\n"
	"  Behavior = CastleBehavior ModuleTag_castle\n"
	"    CastleToUnpackForFaction = Alpha Base_Alpha\n"
	"    InstantUnpack = Yes\n"
	"  End\n"
	"End\n";

struct InstantUnpackFx : ProdWorld
{
	GameLogicDispatch dispatch;
	InstantUnpackFx() : dispatch(*logic)
	{
		load(kInstantUnpackObjects);
		logic->settings().buildRulesLoaded = true;
	}
	Object *makeCastle(const char *name, Player *owner)
	{
		Object *o = make(name, owner ? owner->getDefaultTeam() : players.getNeutralPlayer()->getDefaultTeam());
		REQUIRE(o != nullptr);
		return o;
	}
};

// tryInstantUnpack clears the flag and sets JUST_BUILT when the layout lookup fails
// (no layout registered for the faction's base). RW 0x79C265 clears m_needInstantBuild and sets
// JUST_BUILT unconditionally, and always returns al=1; the caller (update, RW 0x79D0B8) then moves
// the castle to STATE_UNPACKED even though unpack() failed and created nothing. The port matches:
// state becomes STATE_UNPACKED, no owned objects, no footprint.
TEST_CASE("LOCAL-44: instant unpack clears the flag, sets JUST_BUILT and goes to UNPACKED when the layout lookup fails (RW 0x79C265)")
{
	InstantUnpackFx f;
	Object *castle = f.makeCastle("AlphaCastle", f.alice());
	castle->friend_onBuildComplete(); // RW 0x798238: InstantUnpack sets m_needInstantBuild
	CastleBehavior *cb = dynamic_cast<CastleBehavior *>(castle->findModule("CastleBehavior"));
	REQUIRE(cb != nullptr);
	CHECK(cb->needsInstantBuild());
	// No layout registered for "Base_Alpha": the lookup fails on the first update (RW 0x79CF2A ->
	// 0x79C265 -> 0x79BE6A).
	f.logic->runLogicFrame();
	CHECK_FALSE(cb->needsInstantBuild()); // flag cleared unconditionally (RW 0x79C38B)
	CHECK(castle->testModelCondition(Construction::modelConditionIndex("JUST_BUILT"))); // RW 0x79C38F
	// tryInstantUnpack always returns true (RW: al = 1), so update moves to STATE_UNPACKED
	CHECK(cb->state() == CastleBehavior::STATE_UNPACKED);
	CHECK_FALSE(cb->lastError().empty());
	// nothing was created: no footprint, no owned objects
	CHECK(cb->ownedObjects().empty());
}

// A neighbouring valid layout still loads unchanged: the fix to the decoder must not break a
// normal castle unpack.
TEST_CASE("LOCAL-44: a valid neighbouring castle layout still unpacks unchanged")
{
	InstantUnpackFx f;
	CastleTemplate layout;
	layout.name = "Base_Alpha";
	layout.version = 5;
	layout.entries.push_back({ "", "Keep", 0.0f, 0.0f, 0.0f, 0.0f, 40, 1 });
	f.logic->castleTemplates().add(layout);
	Object *castle = f.makeCastle("AlphaCastle", f.alice());
	castle->friend_onBuildComplete();
	CastleBehavior *cb = dynamic_cast<CastleBehavior *>(castle->findModule("CastleBehavior"));
	REQUIRE(cb != nullptr);
	CHECK(cb->needsInstantBuild());
	f.logic->runLogicFrame(); // the castle's first update does the unpack (RW 0x79CF2A -> 0x79C265)
	CHECK(cb->state() == CastleBehavior::STATE_UNPACKED);
	CHECK_FALSE(cb->needsInstantBuild());
	CHECK(castle->testModelCondition(Construction::modelConditionIndex("JUST_BUILT")));
	CHECK(cb->lastError().empty());
	REQUIRE(cb->ownedObjects().size() == 1);
	Object *keep = f.logic->findObjectByID(cb->ownedObjects()[0]);
	REQUIRE(keep);
	CHECK(keep->getTemplate()->getName() == "Keep");
}

} // namespace
