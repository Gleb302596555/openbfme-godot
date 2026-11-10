// PORT-68 (issue #182): Weapon/FXTrigger and OverrideVoiceAttackSound / OverrideVoiceEnterStateAttackSound (weapon template +0x10, +0xD0, +0xD8).
// Synthetic INI and synthetic objects only: no retail data, no ROTWK_INSTALL / OPENBFME_ENNORATH_INSTALL needed.
//
// Expected values come from the disassembly of RotWK 2.01 (game.dat, caveat S-001, read with tools/mod/rw.py):
//   * TheFXTriggerNames RW 0xDA14FC (the Weapon block's FXTrigger list) and the FXList ParticleSysBone list RW 0xD9DE5C hold the same three strings:
//     RW 0xDA08FC and 0xDA14FC both point at 0xBD1584 "NONE", 0xBD155C "CATAPULT_ROCK", 0xBD154C "TREBUCHET_ROCK".
//   * WeaponTemplate::parseOverrideVoice RW 0x73AB45 writes retail's {EVA event id at +0, audio event at +4} pair, and the three token kinds differ in
//     WHICH half they touch: a plain name clears +0 (RW 0x73AC7F `or dword ptr [edi], -1`) and writes +4 (RW 0x73AC54); `+SOUND:` jumps over the clear
//     (RW 0x73AC79) and writes +4 only; `EVA:` writes +0 only (RW 0x73ABEE, no store at +4) after TheEva's lookup RW 0x5DDEA2 (unknown -> RW 0xC25028);
//     NoSound clears both (RW 0x73AB6F / 0x73AB74).
//   * UnitVoiceResponse::pickAndPlayUnitVoiceResponse RW 0x8DDF3B reads that pair at RW 0x8DE0F4 .. 0x8DE150 through Object::getCurrentWeapon RW 0x68B58C
//     (no weapon set +0x374 -> NULL; slot +0x36C indexes the array at +0x354): the slot is +0xD8 for the enter-state messages and +0xD0 otherwise
//     (RW 0x8DE11C `cmp byte ptr [ebp - 0xd], 0`), and the crowd argument is the EMPTY slot built at RW 0x8DE114, so RW 0x8DDA33 tests slot 0 alone
//     (`cmp dword ptr [esi + 4], 0` and `cmp dword ptr [esi], -1`).
//   * Object::doKnockback RW 0x692223 picks the projectile index at RW 0x6922E5 .. 0x692307: `xor edi, edi`, then three AsciiString::compareNoCase
//     RW 0x406585 calls against RW 0xDA08FC (`inc edi` / `cmp edi, 3`), `mov ebx, edi` on a hit; no hit leaves ebx = 0 (= NONE). RW 0x68B56A then
//     stores it at Drawable + 0x360 (RW 0x68B574 / 0x68B578 `mov dword ptr [eax + 0x360], ecx`).
//   * Object's shockwave handler RW 0x6968BC clears Drawable + 0x360 with 0 at entry (RW 0x6968D3 `push 0` / RW 0x6968D7, before the amount and radius
//     tests at RW 0x6968E5 / 0x6968F9) and writes DamageInfo + 0x2C there at RW 0x696E39 (just before the return at RW 0x696E43).
//   * LiveFX's ParticleSysBone creation path RW 0x4C6514 skips an entry whose FXTrigger is neither 0 nor the drawable's +0x360: RW 0x4C662D
//     `cmp dword ptr [ebp - 0x58], edi` (0 = NONE, no test at all) then RW 0x4C6632 `mov eax, [eax + 0x360]` / `cmp eax, [ebp - 0x58]` / `jne 0x4C6D34`.
// GPL-3.0.

#include "doctest.h"
#include "CombatTestUtil.h"
#include "LogicTestUtil.h"

#include "Common/AsciiString.h"
#include "GameClient/ClientEvents.h"
#include "GameClient/Drawable.h"
#include "GameClient/DrawableManager.h"
#include "GameClient/LiveFX.h"
#include "GameClient/UnitVoiceResponse.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDrawModules.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DModelDraw.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/PhysicsBehavior.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Weapon.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace combattest;

namespace
{

// ---- the name list --------------------------------------------------------------------------------------------------------------
// RW 0xDA14FC (Weapon/FXTrigger) and RW 0xD9DE5C (FXList ParticleSysBone): one list of three names, NONE at 0.
TEST_CASE("PORT-68 names: the FXTrigger list is NONE / CATAPULT_ROCK / TREBUCHET_ROCK and ends (RW 0xDA14FC, RW 0xDA08FC)")
{
	REQUIRE(TheFXTriggerNames[0] != nullptr);
	CHECK(std::string(TheFXTriggerNames[0]) == "NONE");
	REQUIRE(TheFXTriggerNames[1] != nullptr);
	CHECK(std::string(TheFXTriggerNames[1]) == "CATAPULT_ROCK");
	REQUIRE(TheFXTriggerNames[2] != nullptr);
	CHECK(std::string(TheFXTriggerNames[2]) == "TREBUCHET_ROCK");
	CHECK(TheFXTriggerNames[3] == nullptr); // RW 0x6922FB `cmp edi, 3`: the scan knows exactly three names
	// the weapon block's list and the FXList's list are the same three strings in retail (the doctest of test_weapon_data pins the raw pointer table)
	CHECK(AsciiStringUtil::compareNoCase("trebuchet_rock", TheFXTriggerNames[2]) == 0);
}

// ---- the client event pipeline ---------------------------------------------------------------------------------------------------
// The logic's call (Object::setDrawableFXTrigger RW 0x68B56A writes Drawable + 0x360) travels the port's SMOOTH-1 seam: the object records one
// FX_TRIGGER event through the client hooks, and the renderer applies it to its drawable.
TEST_CASE("PORT-68 events: setDrawableFXTrigger records one FX_TRIGGER event and the drawable takes the index (RW 0x68B56A)")
{
	logictest::LogicWorld lw;
	REQUIRE(lw.loadError.empty());
	W3DDrawModules::registerTypedDrawModuleData(lw.w.modules);
	REQUIRE(lw.w.load("Object Pfx68Soldier\n"
					  "  KindOf = INFANTRY SELECTABLE\n"
					  "  Draw = W3DDefaultDraw ModuleTag_Draw\n  End\n"
					  "End\n")
				.empty());
	ArchiveW3DFileSource source(lw.w.fx.fsys);
	WW3DAssetManager assets(source);
	DrawableManager dm(assets, *lw.logic);
	ClientEventRecorder rec(*lw.logic);
	lw.logic->setClientHooks(&rec);
	Object *o = lw.make("Pfx68Soldier");
	dm.applyEvents(rec.take());
	Drawable *d = dm.findByObject(o->getID());
	REQUIRE(d);
	REQUIRE(d->getFXTrigger() == 0); // Drawable + 0x360 starts at 0 (NONE)

	o->clientHooks()->setDrawableFXTrigger(*o, 2);
	const std::vector<ClientEvent> events = rec.take();
	REQUIRE(events.size() == 1);
	CHECK(events[0].kind == ClientEvent::FX_TRIGGER);
	CHECK(events[0].object == o->getID());
	CHECK(events[0].fxTrigger == 2);
	dm.applyEvents(events);
	CHECK(d->getFXTrigger() == 2);
	o->clientHooks()->setDrawableFXTrigger(*o, 0);
	dm.applyEvents(rec.take());
	CHECK(d->getFXTrigger() == 0);
	lw.logic->setClientHooks(nullptr);
}

// ---- Object::doKnockback, RW 0x692223 ------------------------------------------------------------------------------------------
// A crush knockback names its projectile type as a string ("NONE" from SquishCollide RW 0x68B5xx, a data name elsewhere); RW matches it against
// TheFXTriggerNames without case and writes the index to the drawable.
struct FxTriggerRecorder : ObjectClientHooks
{
	std::vector<int> triggers;
	std::string log;
	void objectCreated(Object &obj) override
	{
		log += "created#" + std::to_string(obj.getID()) + ";";
		obj.friend_bindToClient(this); // as ClientEventRecorder::objectCreated does: Object + 0xFC, RW 0x628882
	}
	void objectDestroyed(Object &obj) override { log += "destroyed#" + std::to_string(obj.getID()) + ";"; }
	void setDrawableFXTrigger(Object &obj, int fxTrigger) override
	{
		(void)obj;
		triggers.push_back(fxTrigger);
	}
};

const char kPfx68KnockObjects[] =
	"Object Pfx68PunchingBag\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n";

TEST_CASE("PORT-68 knockback: ObjectKnockback::apply maps the projectile name to the FXTrigger index and clears it on an unknown name (RW 0x692223)")
{
	CombatWorld f(kPfx68KnockObjects);
	FxTriggerRecorder hooks;
	f.logic->setClientHooks(&hooks);
	struct Detach { GameLogic *l; ~Detach() { l->setClientHooks(nullptr); } } detach{ f.logic.get() };
	// RW 0x6922E7 .. 0x692302: the three names, compareNoCase; a hit moves the index into the value RW 0x68B56A stores, no hit leaves 0.
	// `apply` stuns the object, so every case takes a fresh victim (a stunned object bails at RW 0x69223E before the store).
	struct Case
	{
		const char *projectile;
		int expected;
	};
	for (const Case &c : { Case{ "CATAPULT_ROCK", 1 }, Case{ "trebuchet_rock", 2 }, Case{ "NONE", 0 }, Case{ "none", 0 }, Case{ "TREBUCHET_ROCKS", 0 },
			 Case{ nullptr, 0 } })
	{
		CAPTURE(c.projectile ? c.projectile : "(null)");
		Object *o = f.unit("Pfx68PunchingBag", 'A', 300.0f, 300.0f);
		REQUIRE(o);
		hooks.triggers.clear();
		REQUIRE(ObjectKnockback::apply(*o, 0.0f, 8.0f, 1.0f, c.projectile));
		REQUIRE(hooks.triggers.size() == 1);
		CHECK(hooks.triggers[0] == c.expected);
		// the knockback itself still runs: the object is stunned and flailing
		CHECK(o->testModelCondition(CombatNames::modelCondition("STUNNED_FLAILING")));
		// a second knockback on the stunned object bails before the store (RW 0x69223E), so nothing new is recorded
		hooks.triggers.clear();
		CHECK_FALSE(ObjectKnockback::apply(*o, 0.0f, 8.0f, 1.0f, "CATAPULT_ROCK"));
		CHECK(hooks.triggers.empty());
	}
}

// ---- Object's shockwave handler, RW 0x6968BC -------------------------------------------------------------------------------------
// Entry clears Drawable + 0x360 with NONE before anything else (RW 0x6968D3 / 0x6968D7); the stun success writes DamageInfo + 0x2C at RW 0x696E39.
// Every other exit of the function (amount or radius 0, no PhysicsBehavior, shockwave resistance, resist >= 1) leaves the cleared value.
DamageInfo shockHit(int fxTrigger)
{
	DamageInfo hit;
	hit.m_input.m_damageType = DAMAGE_CRUSH;
	hit.m_input.m_shockWaveVector = Coord3D{ 3.0f, 4.0f, 0.0f };
	hit.m_input.m_shockWaveAmount = 6.0f;
	hit.m_input.m_shockWaveZMult = 1.5f;
	hit.m_input.m_shockWaveRadius = 10.0f;
	hit.m_input.m_shockWaveTaperOff = 0.75f;
	hit.m_input.m_fxTrigger = fxTrigger;
	return hit;
}

TEST_CASE("PORT-68 shockwave: the handler clears the drawable's FXTrigger at entry and sets DamageInfo +0x2C on the stun (RW 0x6968D7 / 0x696E39)")
{
	CombatWorld f(kPfx68KnockObjects);
	FxTriggerRecorder hooks;
	f.logic->setClientHooks(&hooks);
	struct Detach { GameLogic *l; ~Detach() { l->setClientHooks(nullptr); } } detach{ f.logic.get() };
	Object *a = f.unit("Pfx68PunchingBag", 'A', 300.0f, 300.0f);
	Object *b = f.unit("Pfx68PunchingBag", 'A', 320.0f, 300.0f);
	REQUIRE(a);
	REQUIRE(b);
	hooks.triggers.clear();

	// a hit with no shockwave: amount and radius 0 -> the entry clear alone, then RW 0x696E46's exit
	DamageInfo plain;
	plain.m_input.m_damageType = DAMAGE_CRUSH;
	REQUIRE(ObjectKnockback::shockWave(*a, plain));
	CHECK(hooks.triggers == std::vector<int>{ 0 });

	// a real shockwave: clear at entry, then the hit's own index at the end
	hooks.triggers.clear();
	DamageInfo hit = shockHit(2);
	REQUIRE(ObjectKnockback::shockWave(*b, hit));
	REQUIRE(hooks.triggers.size() == 2);
	CHECK(hooks.triggers[0] == 0);
	CHECK(hooks.triggers[1] == 2);

	// the second hit finds the object already stunned (RW 0x69693B): still the entry clear, and `true` (RW 0x696E46) without the final store
	hooks.triggers.clear();
	DamageInfo again = shockHit(1);
	REQUIRE(ObjectKnockback::shockWave(*b, again));
	CHECK(hooks.triggers == std::vector<int>{ 0 });
}

// ---- LiveFX's ParticleSysBone gate, RW 0x4C662D -----------------------------------------------------------------------------------
// An entry with FXTrigger 0 is never tested (every drawable plays it); otherwise the drawable's +0x360 must be equal or the entry is skipped.
TEST_CASE("PORT-68 ParticleSysBone: the FXTrigger gate reads the drawable's index, and 0 always passes (RW 0x4C662D)")
{
	logictest::LogicWorld lw;
	REQUIRE(lw.loadError.empty());
	W3DDrawModules::registerTypedDrawModuleData(lw.w.modules);
	REQUIRE(lw.w.load("Object Pfx68Gate\n"
					  "  KindOf = INFANTRY\n"
					  "  Draw = W3DDefaultDraw ModuleTag_Draw\n  End\n"
					  "End\n")
				.empty());
	ArchiveW3DFileSource source(lw.w.fx.fsys);
	WW3DAssetManager assets(source);
	DrawableManager dm(assets, *lw.logic);
	ClientEventRecorder rec(*lw.logic);
	lw.logic->setClientHooks(&rec);
	Object *o = lw.make("Pfx68Gate");
	dm.applyEvents(rec.take());
	Drawable *d = dm.findByObject(o->getID());
	REQUIRE(d);

	ParticleSysBoneInfo none;
	none.fxTrigger = 0;
	ParticleSysBoneInfo rock;
	rock.fxTrigger = 1;
	ParticleSysBoneInfo trebuchet;
	trebuchet.fxTrigger = 2;

	CHECK(LiveFX::allowsParticleSysBoneFXTrigger(*d, none)); // the NONE entry is never compared
	CHECK_FALSE(LiveFX::allowsParticleSysBoneFXTrigger(*d, rock));
	CHECK_FALSE(LiveFX::allowsParticleSysBoneFXTrigger(*d, trebuchet));
	d->setFXTrigger(1);
	CHECK(LiveFX::allowsParticleSysBoneFXTrigger(*d, none));
	CHECK(LiveFX::allowsParticleSysBoneFXTrigger(*d, rock));
	CHECK_FALSE(LiveFX::allowsParticleSysBoneFXTrigger(*d, trebuchet));
	d->setFXTrigger(2);
	CHECK_FALSE(LiveFX::allowsParticleSysBoneFXTrigger(*d, rock));
	CHECK(LiveFX::allowsParticleSysBoneFXTrigger(*d, trebuchet));
	lw.logic->setClientHooks(nullptr);
}

// ---- WeaponTemplate::parseOverrideVoice, RW 0x73AB45 -------------------------------------------------------------------------------
// The store is retail's {EVA event at +0, audio event at +4} pair, so the value lines combine instead of replacing one flat "name":
// `EVA:` keeps an earlier sound, `+SOUND:` keeps an earlier EVA, a plain name drops the EVA half, NoSound drops both.
namespace
{
// the parsing side of the weapon store only (the same shape as test_weapon_data's WeaponWorld: a fresh INI environment, the
// `Weapon` block registered, WeaponStores installed as the process-wide store)
struct Pfx68WeaponWorld
{
	initest::Fixture fx;
	WeaponStores stores;
	Pfx68WeaponWorld()
	{
		stores.install();
		fx.env.blocks.registerBlock("Weapon", [](INI *ini) { WeaponStore::parseWeaponTemplateDefinitionGlobal(ini); });
	}
	std::string load(const std::string &text) { return initest::loadError(fx.env, "port68.ini", text); }
	const WeaponTemplate *find(const char *name)
	{
		const WeaponTemplate *t = stores.weapons().findWeaponTemplate(name);
		REQUIRE_MESSAGE(t != nullptr, name);
		return t;
	}
};
} // namespace

TEST_CASE("PORT-68 weapon voice: the override slots are an {EVA, sound} pair and each line writes its own half (RW 0x73AB45)")
{
	Pfx68WeaponWorld w;
	REQUIRE(w.load("Weapon TwoHalves\n"
				   "  OverrideVoiceAttackSound = SwordHitSound\n"
				   "  OverrideVoiceAttackSound = EVA:AttackEva\n"
				   "End\n"
				   "Weapon PlusSound\n"
				   "  OverrideVoiceEnterStateAttackSound = EVA:EnterAttackEva\n"
				   "  OverrideVoiceEnterStateAttackSound = +SOUND:BowReleaseSound\n"
				   "End\n"
				   "Weapon PlainThenPlain\n"
				   "  OverrideVoiceAttackSound = EVA:FirstEva\n"
				   "  OverrideVoiceAttackSound = SecondSound\n"
				   "End\n"
				   "Weapon NoSoundAfter\n"
				   "  OverrideVoiceAttackSound = OldSound\n"
				   "  OverrideVoiceAttackSound = NoSound\n"
				   "End\n")
			.empty());
	// `EVA:` alone keeps the sound an earlier line set (RW 0x73ABEE writes +0 only) and is the last token
	const WeaponTemplate::VoiceOverride &two = w.find("TwoHalves")->m_overrideVoiceAttackSound;
	CHECK(two.eva == "AttackEva");
	CHECK(two.sound == "SwordHitSound");
	CHECK(two.isEva);
	// `+SOUND:` writes +4 and skips the +0 clear (RW 0x73AC79 / 0x73AC7F): the EVA id survives
	const WeaponTemplate::VoiceOverride &plus = w.find("PlusSound")->m_overrideVoiceEnterStateAttackSound;
	CHECK(plus.eva == "EnterAttackEva");
	CHECK(plus.sound == "BowReleaseSound");
	CHECK_FALSE(plus.isEva);
	CHECK(plus.plusSound);
	// a plain name clears +0 and writes +4: the EVA half of an earlier line is gone
	const WeaponTemplate::VoiceOverride &plain = w.find("PlainThenPlain")->m_overrideVoiceAttackSound;
	CHECK(plain.eva.empty());
	CHECK(plain.sound == "SecondSound");
	CHECK_FALSE(plain.plusSound);
	// NoSound clears both halves (RW 0x73AB6F / 0x73AB74)
	const WeaponTemplate::VoiceOverride &none = w.find("NoSoundAfter")->m_overrideVoiceAttackSound;
	CHECK(none.eva.empty());
	CHECK(none.sound.empty());
	CHECK(none.name.empty());
	CHECK_FALSE(none.isEva);
}

// ---- UnitVoiceResponse::attackVoices, RW 0x8DE0F4 .. 0x8DE150 ----------------------------------------------------------------
// The current weapon's slot decides the attack voice: +0xD8 for the enter-state messages (RW 0x8DE126), +0xD0 otherwise (RW 0x8DE132).
namespace
{
struct Pfx68Sink : UnitVoiceResponse::Sink
{
	std::set<std::string> known;
	std::vector<std::string> sounds, eva;
	bool soundExists(const std::string &name) override { return known.count(name) != 0; }
	void playSound(const std::string &name, ObjectID, int) override { sounds.push_back(name); }
	void reportEva(const std::string &name, const Coord3D *) override { eva.push_back(name); }
	void clear()
	{
		sounds.clear();
		eva.clear();
	}
};

// Two weapons of one object: PRIMARY (the unset set) speaks its own override, SECONDARY (WEAPONSET_TOGGLE_1) another. The unit's own
// VoiceAttack / VoiceEnterStateAttack would say "UnitVoiceAttack*" if the weapon rule were missing.
const char kPfx68VoiceObjects[] =
	"Object Pfx68Bombardier\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  VoicePriority = 10\n"
	"  VoiceAttack = UnitVoiceAttack\n"
	"  VoiceEnterStateAttack = UnitVoiceEnterStateAttack\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY Pfx68Sword\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = WEAPONSET_TOGGLE_1\n"
	"    Weapon = PRIMARY Pfx68Firebomb\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n";

const char kPfx68VoiceWeapons[] =
	"Weapon Pfx68Sword\n"
	"  AttackRange = 11.5\n"
	"  MeleeWeapon = Yes\n"
	"  DelayBetweenShots = 1000\n"
	"  OverrideVoiceAttackSound = SwordVoiceAttack\n"
	"  OverrideVoiceEnterStateAttackSound = SwordVoiceEnterStateAttack\n"
	"  DamageNugget\n"
	"    Damage = 10\n"
	"    Radius = 0.0\n"
	"    DelayTime = 0\n"
	"    DamageType = SLASH\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	"Weapon Pfx68Firebomb\n"
	"  AttackRange = 60\n"
	"  DelayBetweenShots = 1000\n"
	"  OverrideVoiceAttackSound = FirebombVoiceAttack\n"
	"  OverrideVoiceEnterStateAttackSound = FirebombVoiceEnterStateAttack\n"
	"  DamageNugget\n"
	"    Damage = 20\n"
	"    Radius = 0.0\n"
	"    DelayTime = 0\n"
	"    DamageType = PIERCE\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n";
} // namespace

TEST_CASE("PORT-68 unit voice: the current weapon's OverrideVoiceAttackSound speaks over the unit's VoiceAttack (RW 0x8DE0F4)")
{
	// the WeaponSet names resolve through TheWeaponStore while the object block is parsed (the same order as a mod's weapon.ini before object.ini),
	// so the weapons are loaded first and the object is parsed once
	CombatWorld f;
	REQUIRE(f.w.load(kPfx68VoiceWeapons, INI_LOAD_OVERWRITE, "port68_weapon.ini").empty());
	REQUIRE(f.w.load(kPfx68VoiceObjects, INI_LOAD_OVERWRITE, "port68_object.ini").empty());
	Pfx68Sink sink;
	for (const char *s : { "UnitVoiceAttack", "UnitVoiceEnterStateAttack", "SwordVoiceAttack", "SwordVoiceEnterStateAttack", "FirebombVoiceAttack",
			 "FirebombVoiceEnterStateAttack" })
	{
		sink.known.insert(s);
	}
	UnitVoiceResponse voice(*f.logic, sink);
	voice.setLocalPlayerIndex(f.playerIndex("Alice"));
	Object *u = f.unit("Pfx68Bombardier", 'A', 300.0f, 300.0f);
	REQUIRE(u);
	REQUIRE(u->getWeapons());
	REQUIRE(u->getWeapons()->currentWeapon());
	REQUIRE(u->getWeapons()->currentWeapon()->getTemplate() == TheWeaponStore->findWeaponTemplate("Pfx68Sword"));

	// the PRIMARY weapon's pair: +0xD0 for an attack command
	CHECK(voice.pickAndPlay({ u->getID() }, MSG_DO_ATTACK_OBJECT));
	CHECK(sink.sounds == std::vector<std::string>{ "SwordVoiceAttack" });
	// +0xD8 for the enter-state message (RW 0x8DE11C: the enter-state byte chooses the slot, then the message type does not matter)
	sink.clear();
	CHECK(voice.pickAndPlay({ u->getID() }, UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_ATTACK));
	CHECK(sink.sounds == std::vector<std::string>{ "SwordVoiceEnterStateAttack" });

	// the weapon set change (RW 0x6C99E2) makes the SECONDARY template current: its own pair speaks, the unit's rows stay silent
	u->getWeapons()->setWeaponSetFlag(CombatNames::weaponSetBit("WEAPONSET_TOGGLE_1"), true);
	REQUIRE(u->getWeapons()->currentWeapon()->getTemplate() == TheWeaponStore->findWeaponTemplate("Pfx68Firebomb"));
	sink.clear();
	CHECK(voice.pickAndPlay({ u->getID() }, MSG_DO_ATTACK_OBJECT));
	CHECK(sink.sounds == std::vector<std::string>{ "FirebombVoiceAttack" });
	sink.clear();
	CHECK(voice.pickAndPlay({ u->getID() }, UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_ATTACK));
	CHECK(sink.sounds == std::vector<std::string>{ "FirebombVoiceEnterStateAttack" });
}

// ---- the whole path: Weapon/FXTrigger -> DamageInfo -> shockwave -> drawable -> ParticleSysBone gate --------------------------
// FillDamageInfo (RW 0x90E28C step 9) copies the weapon's +0x10 into DamageInfo + 0x2C; the victim's shockwave handler writes DamageInfo + 0x2C
// to Drawable + 0x360 (RW 0x696E39) after clearing it at entry; the ParticleSysBone gate of RW 0x4C662D then lets exactly the CATAPULT_ROCK
// entries through. The parsed weapon block and the real damage core are used; only the objects are synthetic.
namespace
{
// the minimal NuggetDamageHost of a plain direct hit (every modifier absent, no contain, no flank, no self hit)
struct Pfx68NuggetHost : NuggetDamageHost
{
	Coord3D at{};
	bool hasVictim() override { return true; }
	float victimDistanceSqr2D(const Coord3D &) override { return 0.0f; }
	Coord3D victimPosition() override { return at; }
	Coord3D sourcePosition() override { return at; }
	bool sourceHasContain() override { return false; }
	int sourcePassengerCount() override { return 0; }
	bool filterAllowsVictim(const ObjectFilter &) override { return false; }
	bool sourceAdditive(int, float &) override { return false; }
	bool sourceMultiplicative(int, bool, float &out) override
	{
		out = 1.0f;
		return false;
	}
	bool victimFlankedBySource() override { return false; }
	bool sourceFlankedByVictim() override { return false; }
	bool weaponSourceIsVictim() override { return false; }
	std::uint32_t sourcePlayerMask() override { return 1u; }
	unsigned weaponSourceID() override { return 1u; }
};
} // namespace

TEST_CASE("PORT-68 path: the weapon's FXTrigger reaches the damage record, the shockwave writes it to the drawable, the gate selects the bones (RW 0x90E28C -> 0x696E39 -> 0x4C662D)")
{
	CombatWorld f(kPfx68KnockObjects);
	REQUIRE(f.w.load("Weapon Pfx68CatapultHit\n"
					 "  AttackRange = 11.5\n"
					 "  FXTrigger = CATAPULT_ROCK\n"
					 "  MetaImpactNugget\n"
					 "    ShockWaveAmount = 6.0\n"
					 "    ShockWaveRadius = 20.0\n"
					 "    ShockWaveTaperOff = 0.75\n"
					 "  End\n"
					 "  DamageNugget\n"
					 "    Damage = 10\n"
					 "    Radius = 0.0\n"
					 "    DamageType = PIERCE\n"
					 "    DeathType = NORMAL\n"
					 "  End\n"
					 "End\n",
				 INI_LOAD_OVERWRITE, "port68_fxtrigger.ini")
				.empty());
	const WeaponTemplate *catapult = TheWeaponStore->findWeaponTemplate("Pfx68CatapultHit");
	REQUIRE(catapult);
	REQUIRE(catapult->m_fxTrigger == 1); // FXTrigger CATAPULT_ROCK = TheFXTriggerNames[1]

	// FillDamageInfo (RW 0x90E28C step 9): in.m_fxTrigger = the weapon's +0x10
	const DamageNugget *damage = nullptr;
	for (const std::shared_ptr<WeaponNugget> &n : catapult->m_nuggets)
	{
		if (n->kind() == NUGGET_DAMAGE)
		{
			damage = static_cast<const DamageNugget *>(n.get());
		}
	}
	REQUIRE(damage);
	Pfx68NuggetHost host;
	DamageInfo record;
	REQUIRE(FillDamageInfo(*damage, *catapult, host, false, nullptr, record));
	REQUIRE(record.m_input.m_fxTrigger == 1);

	FxTriggerRecorder hooks;
	f.logic->setClientHooks(&hooks);
	struct Detach { GameLogic *l; ~Detach() { l->setClientHooks(nullptr); } } detach{ f.logic.get() };
	Object *victim = f.unit("Pfx68PunchingBag", 'A', 300.0f, 300.0f);
	REQUIRE(victim);
	hooks.triggers.clear();

	// the shockwave half of the record, then the handler: the entry clear, then the record's own index
	DamageInfo hit = record;
	hit.m_input.m_shockWaveVector = Coord3D{ 1.0f, 0.0f, 0.0f };
	hit.m_input.m_shockWaveAmount = 6.0f;
	hit.m_input.m_shockWaveRadius = 20.0f;
	hit.m_input.m_shockWaveTaperOff = 0.75f;
	REQUIRE(ObjectKnockback::shockWave(*victim, hit));
	REQUIRE(hooks.triggers.size() == 2);
	CHECK(hooks.triggers[0] == 0); // RW 0x6968D7, the entry clear
	CHECK(hooks.triggers[1] == 1); // RW 0x696E39, DamageInfo + 0x2C
	CHECK(hooks.triggers[1] == record.m_input.m_fxTrigger);
}

// The gate itself: with the drawable at 1 the CATAPULT_ROCK entries play and the TREBUCHET_ROCK ones do not; a NONE entry always plays.
TEST_CASE("PORT-68 path: the ParticleSysBone gate selects the entries whose FXTrigger is NONE or the drawable's own (RW 0x4C662D)")
{
	ParticleSysBoneInfo none;      none.fxTrigger = 0;
	ParticleSysBoneInfo catapult;  catapult.fxTrigger = 1;
	ParticleSysBoneInfo trebuchet; trebuchet.fxTrigger = 2;
	// the drawable is the only input besides the entry, so a bare object of the port's drawable type stands in: the FXTrigger field is all that is read
	logictest::LogicWorld lw;
	REQUIRE(lw.loadError.empty());
	W3DDrawModules::registerTypedDrawModuleData(lw.w.modules);
	REQUIRE(lw.w.load("Object Pfx68GateVictim\n"
					  "  KindOf = INFANTRY\n"
					  "  Draw = W3DDefaultDraw ModuleTag_Draw\n  End\n"
					  "End\n")
				.empty());
	ArchiveW3DFileSource source(lw.w.fx.fsys);
	WW3DAssetManager assets(source);
	DrawableManager dm(assets, *lw.logic);
	ClientEventRecorder rec(*lw.logic);
	lw.logic->setClientHooks(&rec);
	Object *o = lw.make("Pfx68GateVictim");
	dm.applyEvents(rec.take());
	Drawable *d = dm.findByObject(o->getID());
	REQUIRE(d);
	for (int trigger : { 0, 1, 2 })
	{
		CAPTURE(trigger);
		d->setFXTrigger(trigger);
		CHECK(LiveFX::allowsParticleSysBoneFXTrigger(*d, none));
		CHECK(LiveFX::allowsParticleSysBoneFXTrigger(*d, catapult) == (trigger == 1));
		CHECK(LiveFX::allowsParticleSysBoneFXTrigger(*d, trebuchet) == (trigger == 2));
	}
	lw.logic->setClientHooks(nullptr);
}

} // namespace
