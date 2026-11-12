# Port 148 — Drogoth does not attack a fortress while flying (P-59)

Status: analysis in progress; no engine behaviour changed yet.

## Sources

- RotWK 2.01 `game.dat`, md5 `daed3668006cd90f01c34e5a7da1901f`, read with `tools/mod/rw.py dis` (caveat S-001).
- DONOR BFME2 1.06: `Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate/AIGiantBirdSwoopStateOnEnter.cpp`, `AIUpdateInterfaceAttackObjectCommands.cpp`.
- Ennorath INI 1.9.8.2: `Drogoth`, `WildDrogothnew`, fell beasts and eagles. The active `Drogoth` WeaponSet uses `NazgulPlowAttack` / `NazgulClawAttack`; `WildDrogothnew` uses `DrogorhPlowAttack` / `DrogothClawAttack`. No mod values are copied here.

## RW chain read so far

- RW 0x76D995 `GiantBirdAIUpdate::aiDoCommand`: commands 0xB/0xC/0x39 go to RW 0x76A139 (attack-object swoop); 0xE attack-position is ignored; 0xF attack-move goes to RW 0x76A09D. While `+0x5FC` attacking, the command is deferred through RW 0x75307D.
- RW 0x76DB18 `AIGiantBirdSwoopState::onEnter`: clears flight bits 3..5 and victim; a PRODUCED_AT_HELIPAD goal is rejected; horde goals are adjusted through RW 0x68D30E; RW 0x76DCB5 calls Object::chooseBestWeaponForTarget(victim, 2, CMD_FROM_AI) through RW 0x68B619 -> RW 0x6C8A4E. No current weapon -> failure. Grab/machine/clip bits are set at RW 0x76DCD1..0x76DD44; the contact point is asked at RW 0x76DDD9 / 0x76DE1B / 0x76DE4C; the path masks are passed at RW 0x76DECD..0x76DEF5.
- RW 0x76B668 `AIGiantBirdAttackState::onEnter`: sets attacking; clip attacks wait for update; the victim contact point is asked at RW 0x76B70C / 0x76B73F; the gate at RW 0x76B744..0x76B794 is `max(speed*3, 8.5)` squared against distance to that contact point, not the weapon's AttackRange. The ordinary shot is RW 0x76B8BD -> 0x69036C (firing condition) -> RW 0x76B8CA -> 0x69200A (fire current weapon at the victim).
- RW 0x69200A: refuses a missing current weapon or a non-ready weapon (RW 0x6CDCE7), then fires through RW 0x6CF3AE / 0x6CF328; the bird wrapper itself has no AttackRange gate.
- RW 0x6C85CA `getVictimAntiMask`: a ground STRUCTURE is `GROUND | STRUCTURE` (0x102), other ground targets `GROUND` (0x2); airborne targets use the airborne bits. RW 0x6C8A4E scores OnlyAgainst / PreferredAgainst / OnlyInCondition and the ready/backup best.

## Not yet established

The first differing port step is not yet proven. The port's bird attack gate already matches the RW speed/contact-point shape above; the remaining comparison is weapon choice / applicability against a structure, contact-point and path arrival, and the synthetic reproduction. No INFERENCE is being ported.
