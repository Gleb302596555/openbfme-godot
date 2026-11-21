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
| the defensive stance no longer stops units | `.stancem` 0xF34000 | RW 0x862161 in StancesBehavior::setStance (RW 0x8620DD) | retail: a stance 3 (HoldGround) request idles the AI (RW 0x86216B: `aiIdle`, RW 0x5E821A(0)) whatever it is doing. Patched: only when the AI (Object + 0x260) is not waiting for a path (byte AI + 0x3B1) and is idle (AI vslot 0x1B8, isIdle RW 0x6643FC); a moving unit keeps its order | retail behaviour (`StancesBehavior.cpp`, RW 0x86216B) |
| the middle mouse button resets the camera again | `.camfix` 0xF37000, 0xF37029; in place RW 0x83ADE6 | RW 0x83AD3D (middle up), RW 0x83AEA3 (mouse move while the middle button is held) | retail (LookAtTranslator RW 0x83AC4A): a click is a release within 5 client frames of the press (RW 0x83AD62 .. 0x83AD73) whose release point is within 5 pixels (RW 0x83AD3D .. 0x83AD5C; the y difference is compared unsigned, so any move upwards is no click). Patched: the press stores `timeGetTime` (winmm, import 0xBD0920) at + 0x40 instead of the frame; a move more than 5 pixels from the press point on either axis sets bit 2 of + 0x39; the release resets the camera when + 0x39 was exactly 1 (pressed, never moved), + 0x38 (scrolling) is clear and at most 350 ms passed | retail behaviour (`LookAtXlat.cpp`, RW 0x83AD16) |
| war wagons and the Witch-king can unload through the common evacuate | not located yet | — | the mod's objects: `DwarvenBattleWagon` and `MordorWitchKingOnFellBeast` (`TransportContain`), `DwarvenBattleWagon_Eisenberge` (`HordeTransportContain`) | not decoded |
| map search in the lobby | `.mapfind` 0xF33000, `.mapsvar` | RW 0x49FC37, 0x722745, 0x840879, 0x840906, 0x8465EA; call RW 0x8467A8 | INFERENCE (name and hooks in the map list and text entry code) | — |
| basic hotkeys in the options | `.hotkeys`, `.keydef`, `.keycmd` | RW 0x5DA8F5, 0x812C89, 0x812EDA, 0x81FAAB; RW 0x5DA910, 0x99106B | `.hotkeys` 0xF383CD adds default CommandMap entries (meta 0x5F on key 0xC8) when the map has none; the rest INFERENCE | — |
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
