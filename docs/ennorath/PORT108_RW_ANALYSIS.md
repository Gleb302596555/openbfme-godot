# PORT-108: CommandButton DoubleClick (issue #268)

Verdict for RotWK 2.01: parsed, no identified behavioral reader. This is a
provenance stop, not a request to invent a double-click gate. No engine behavior
or tests were changed. The owner supplied the `.text` byte-access scan in the
issue; its limits are noted below.

## Button and click path

- RW 0x5DA711 parses CommandButton: an absent name calls create RW 0x71C439,
  which allocates 0x2E0 bytes and calls ctor RW 0x75D516 at 0x71C461. The
  ctor installs vtable RW 0xC2C890 and clears byte +0x100 at RW 0x75D650.
  The table RW 0xC2BC98 contains name RW 0xC2C034, bool parser RW 0x75CAA4,
  and offset +0x100. Parse path RW 0x5DA7B6 uses table RW 0xC2BAC8.
  The vtable's first entry RW 0x75D92D invokes destructor RW 0x75D7BD;
  that destructor releases the other members without reading +0x100.
- RW 0x9D2814 forwards a press as 0x4008; RW 0x941BA0..0x941C29
  obtains the command button, passes `message == 0x400B` and
  `message != 0x4009` to processCommandUI RW 0x940435, and refreshes the
  image. RW 0x940498 checks IsClickable (+0x105), and RW 0x9404A1..0x9404AB
  rejects repeated commands other than command type 3. Neither checks the
  DoubleClick byte. The owner's byte scan found no +0x100 byte read in the
  entire ControlBar range 0x940000..0x948000 or CommandButton range
  0x75C000..0x75F000 (the latter contains only the ctor write).
- DONOR BFME2 0x0053B1CF and 0x00539A65
  (`Code/GameEngine/Source/GameClient/GUI/ControlBar/ControlBarCommandProcessing.cpp`)
  show the equivalent click forwarding and command handling. Donor field table
  (`Code/GameEngine/Source/Common/INI/INICommandButton.cpp`), constructor
  (`Code/GameEngine/Source/Common/Rva0035BB4BCtor.cpp`) and declaration
  (`Code/GameEngine/Include/GameClient/CommandButton.h`) contain the field;
  `git grep` for `m_doubleClick` and `doubleClick` finds no button reader.

## Remaining byte-read candidates

- RW 0x741277 reads `[ecx+4]+0x100`; its adjacent getters RW 0x741263 and
  0x74126D read +0xF8 and +0xFC from the same nested record. Locomotor table
  RW 0xBF4868 identifies +0x100 as `BurningDeathIsCavalry`, with adjacent
  `WanderAboutPointRadius` and `BurningDeathRadius` at +0xF8/+0xFC. No xref
  to 0x741277 was found. The identical offsets are in DONOR BFME2
  `Code/GameEngine/Source/Common/Rva001E72B4Reload.cpp`: this is a locomotor
  template accessor, not an established CommandButton reader.
- RW 0x748418 follows owner -> AI -> current locomotor -> template at
  0x748422..0x748441, then reads +0x100 and float +0xFC at 0x748444;
  the table RW 0xBF4868 names them `BurningDeathIsCavalry` and
  `BurningDeathRadius`. This is AIBurningDeathState's next goal, not a button.
- RW 0x880B9C tests its own byte at 0x880BA9, then processes a contained
  object and its horde members; RW 0xC5D9E4 is its HordeSiegeEngineContain
  interface vtable entry, with the source path string beside it at RW
  0xC5D9AC. It is the contain exit path, not CommandButton.
- RW 0x9C66E0 / 0x9C6D82 / 0x9C6E50 / 0x9C6F28 read +0x100 from
  the global at RW 0xDE36C4 and pass +0xFC with it to font size adjustment
  RW 0x5E9BEF. The `CreditsNormalFont` entry RW 0xBF4DC0 is at +0xF8
  in GlobalLanguage (font name, point size, bold); RW 0x9C667F loads
  Credits.ini. These are credits font reads, not command buttons. DONOR
  `Code/GameEngine/Source/GameClient/GlobalLanguage.cpp` and `Credits.cpp`
  identify the matching font descriptor and credits load.
- RW 0x821402 is an experience level record, not a button (owner's scan in
  issue #268; model-condition and level-grant path RW 0x821319).

## Scope and limitation

The scan counted 87 byte accesses with displacement +0x100 across `.text`,
including stack locals and W3D/library code. It does not alone rule out a
word/dword read spanning the byte or an access via a pre-adjusted pointer.
The conclusion combines it with the specific button construction, destructor,
parse, click-path and donor checks above; no such indirect reader was established.
Revisit S-7292 if a reachable RW instruction reading the button field is found.
