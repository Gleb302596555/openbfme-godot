# Ennorath's patched game.dat: what each pySAGE edit changes

Ennorath ships its own `RotWK\game.dat`, patched with pySAGE (1.9.8.5: md5 `fc32e4c4142b5c607b90972eaf448400`). OpenBFME never runs it; the
profile «Ennorath 1.9.8.5 game.dat» (`Common/ModEngineFeatures.h`, lane MODS-3) names it, and each edit the owner decides to take becomes one
`ennorath.*` feature of that profile (PROCESS 1: a departure from 2.01 waits for the owner's word; PROCESS 12: fork only, never upstream).

The RW reference stays the vanilla 2.01 `game.dat` (md5 `daed3668…`). The patched binary keeps the retail sections at the same addresses and
adds one section per edit; an edit is a `jmp` / `call` from the retail code into its section, or bytes changed in place. Below, `RW 0x…` is the
retail site, `PYSAGE 1.9.8.5 .<section> 0x…` the patched code. How it was read (2026-11-21): a byte diff of the two `.text` sections, the
`jmp` / `call` targets that land in each new section, and the disassembly of both sides (`tools/mod/rw.py dis` on each file). The 1.9.8.4
binary is gone (the owner's `RotWK/` folder was replaced), so an edit's version is taken from the patch note, not from a diff.

## 1.9.8.5 edits (patch note section 1.9.8.5)

| patch note | section | hooked from (retail) | what the patched code does | port |
|---|---|---|---|---|
| the defensive stance no longer stops units | `.stancem` 0xF34000 | RW 0x862161 in StancesBehavior::setStance (RW 0x8620DD) | retail: a stance 3 (HoldGround) request idles the AI (RW 0x86216B: `aiIdle`, RW 0x5E821A(0)) whatever it is doing. Patched: only when the AI (Object + 0x260) is not waiting for a path (byte AI + 0x3B1) and is idle (AI vslot 0x1B8, isIdle RW 0x6643FC); a moving unit keeps its order | `ennorath.stance-keeps-moving` (`StancesBehavior::setStance`); off: retail RW 0x86216B. Tests `MODS-3: ennorath.stance-keeps-moving*` |
| the middle mouse button resets the camera again | `.camfix` 0xF37000, 0xF37029; in place RW 0x83ADE6 | RW 0x83AD3D (middle up), RW 0x83AEA3 (mouse move while the middle button is held) | retail (LookAtTranslator RW 0x83AC4A): a click is a release within 5 client frames of the press (RW 0x83AD62 .. 0x83AD73) whose release point is within 5 pixels (RW 0x83AD3D .. 0x83AD5C; the y difference is compared unsigned, so any move upwards is no click). Patched: the press stores `timeGetTime` (winmm, import 0xBD0920) at + 0x40 instead of the frame; a move more than 5 pixels from the press point on either axis sets bit 2 of + 0x39; the release resets the camera when + 0x39 was exactly 1 (pressed, never moved), + 0x38 (scrolling) is clear and at most 350 ms passed | `ennorath.middle-click-camera` (`LookAtXlat.cpp`; the port's clock is the client frame's, 33 ms steps); off: retail RW 0x83AD16. Tests `camera: ennorath.middle-click-camera*` |
| war wagons and the Witch-king can unload through the common evacuate (owner: «evacuate all» of a dwarf tunnel, or of the Angmar crypts with the dark magic subfaction, left the wagons and the Angmar Witch-king inside: each had to be clicked out) | `.keydef` 0xF39580 (the section also holds key code) | RW 0x99106B in the horde garrison's evacuate all RW 0x991027 (TunnelContain vtable RW 0xC5DCE0 + 0x80 = RW 0x87CE8F -> RW 0x991027) | retail: per rider, its contain (Object + 0x258): a horde (contain vslot 0x7C) gets vslot 0x84; a contain that is no horde -> the rider is skipped (RW 0x991072 je 0x99109E); no contain -> aiExit (RW 0x991099 -> 0x7716C1). Patched: the no-horde case goes to RW 0x991086 (aiExit) too. The mod's riders with such a contain: `DwarvenBattleWagon` (`TransportContain`), `DwarvenBattleWagon_Eisenberge` (`HordeTransportContain`: the horde test decides), `AngmarWitchking_mod` (`CitadelSlaughterHordeContain`) | `ennorath.evacuate-containers` (`GarrisonContain::orderAllPassengersToExit`); off: retail. Test `MODS-3: ennorath.evacuate-containers*`. Found beside it: a plain GarrisonContain runs the same body where RotWK uses RW 0x867D6A, stop S-7715 |
| map search in the lobby | `.mapfind` 0xF33000, `.mapsvar` | RW 0x49FC37, 0x722745, 0x840879, 0x840906, 0x8465EA; call RW 0x8467A8 | INFERENCE (name and hooks in the map list and text entry code) | — |
| basic hotkeys in the options | `.hotkeys`, `.keydef`, `.keycmd` | RW 0x5DA8F5, 0x812C89, 0x812EDA, 0x81FAAB; RW 0x5DA910 (RW 0x99106B is the evacuate above) | `.hotkeys` 0xF383CD adds default CommandMap entries (meta 0x5F on key 0xC8) when the map has none; the rest INFERENCE | — |
| option to switch the online lag fix | `.dlyfix`, `.optfix`; in place RW 0x62ECC8, 0x632606, 0x632633, 0x63264C, 0x632AD1 (frame counts 6 / 10 -> 12) | RW 0x632535, 0x632A9B | INFERENCE | — |

## Earlier edits (1.9.8.3, 1.9.8.4, by the patch note)

| section | hooked from | what (patch note / reading) |
|---|---|---|
| `.fxrate`, `.fpsvar`, `.fpsdraw`; data RW 0xD9F60C (30 -> 60) | RW 0x5F515F; calls RW 0x648861, 0x6488BD | 30 / 60 FPS: not taken (owner 2026-11-15, OpenBFME keeps its frame rate) |
| `.txtclip` | RW 0x722A8A | the text entry clipboard Ctrl+A / C / X / V: P-128, kept in branch `agent/claude/p128-chat-hotkeys` |
| `.cmdext` | RW 0x943E11; in place the CommandSet size 0xA0 -> 0x11C (RW 0x71C4CC, 0x80C8D4 ..) | 64 CommandSet slots (the port has `edain-unchained.commandset-limit`; Ennorath's data does not need it) |
| `.upg5k` | 70 sites (the upgrade mask, push 0x480 -> 0x1400) | 5120 upgrades instead of 1152 (S-5303; Ennorath's data stays under 1152) |
| `.rpchat`, `.rppause`, `.rppvar` | RW 0x77DCFC .. 0x9179DE, RW 0x6325D5, 0x63F1B9, 0x77F66B | replays: tabs, chat, pause Ctrl+] |
| `.chatmut` | RW 0x62D232, 0x826CC1, 0x8D55ED, 0x8D560B, 0x915E00, 0x9162F5 | chat block online |
| `.bldgrp`, `.padsel`, `.bldext`, `.bldcan`, `.bldlvl`, `.bldcap` | RW 0x71ED96, 0x625696, 0x6A286A, 0x6A2955, 0x940975, 0x930E21, 0x941BFC, 0x940435, 0x942733, 0x71DAB7, 0x944834, 0x9448EB | select all buildings (`.padsel` compares expansion pads by their `ExpansionPad` + `Side` / `Corner` names), the building limit fix |
| `.optui` | RW 0x5DA8F0, 0x5ED37A, 0x639FEF, 0x6E573C, 0x9204D9, 0x920602, 0x9212BA; `.rdata` RW 0xC1B4B4 | every resolution in the options |
| `.wotrui`, `.wotrobs`, `.wtrview`, `.wtrdisp`, `.lobfac`; in place the player count 6 -> 8 (RW 0x840A91, 0x84692B), RW 0x6BEBE5 | RW 0x841691, 0x627C41 .., 0x62FE9C, 0x7BAABE .., RW 0x84AB5E .. | War of the Ring for 8 players |
| `.obscmd` 0xED8000 | call RW 0x941BD2 (processContextSensitiveButtonClick) | an observer may press commands 0x37 / 0x38 (PUSH / POP_VISIBLE_COMMAND_RANGE): the same rule as `edain-unchained.observer-command-range` |
| `.spstore`, `.ressort` | RW 0x822890, 0x8231CA; RW 0x4451CE (and in place RW 0x4451BB, 0x445229) | INFERENCE: the spell store, a resolution / resource sort |

## Disassembly of the sections for a task (2026-11-22)

The dossier (`refs/arena/dossier/<issue>`) holds vanilla 2.01 functions only. For a task porting a pySAGE edit, Claude runs
`tools/mod/pysage_dump.py <patched game.dat> <vanilla game.dat> <out> <sections> <hook RW,...> [<.rdata RW,...>] [--ver 1.9.8.5]` on the
PC (the 1.9.8.5 game.dat is in the owner's Ennorath folder `docs/ennorath/RotWK/`, kept out of git by `.git/info/exclude`) and puts the
file on the agent's sandbox as `/content/sandbox/work-dossier-<issue>-<name>.txt` and on Drive `openbfme-runs/dossiers/`; never in plain
text on GitHub. Done: #811 (`.optui .optfix .keycmd .dlyfix .hotkeys .keydef`), #812 (`.mapfind .mapsvar`), #813 (`.chatmut`).
