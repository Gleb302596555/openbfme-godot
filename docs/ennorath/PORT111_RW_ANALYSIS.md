# PORT-111 — `FXParticleSystem/System/UseMaximumHeight` in RotWK 2.01

## Verdict

**STOP: no reachable RotWK runtime reader was found for this field.** No engine code or `PORT-111*` behavior test is added: deriving a maximum-height rule from the issue title or from a field name would invent behavior that the executable does not perform. Whether the intended scope is instead a new, non-retail feature needs the owner's decision.

The checked `game.dat` is the community-modified image recorded by project stop S-001; addresses below refer to that image.

## Field registration and data class

`python tools/mod/rw.py game.dat str UseMaximumHeight` resolves the name to RW `0xBF7710`, with code references at `0x5F3631` and `0x5FAEDD` (both addresses are inside immediate operands, not instruction starts).

- The stack-built `FXParticleSystem/System` field table at RW `0x5F35D0` pairs the name at `0xBF7710` with byte offset `+0x83`; the table is passed to the INI initializer at RW `0x42DB80`. This is parsing, not a runtime branch.
- The code around RW `0x5FAE70` passes `&object[+0x83]` and the same name to RW `0x5F817B`. That helper tests the byte and, only when nonzero, calls the INI-output helper at RW `0x5F7B96`; the zero branch returns without output. This is the `writeINI` path, not particle placement. DONOR BFME2 `Code/GameEngine/Source/GameClient/System/FXParticleSystem/ParticleSystemTemplateWriteINI.cpp` has the matching `UseMaximumHeight` writer (`Rva001F89C3Write`), not a runtime use.
- Template creation at RW `0x5FB981` allocates `0xD4` bytes and calls the constructor at RW `0x5FB907`. That constructor calls the base constructor at RW `0x5F44F4`, then installs template vtable RW `0xBF7B60`; the base vtable is RW `0xBF7868`. The base constructor initializes the bool at `+0x83` to zero at RW `0x5F4576`. The template vtable's xfer slot (slot 3) is RW `0x5F461D`.
- RW `0x5F3664` copies the `+0x83` byte at `0x5F371F..0x5F3725`. Other copies do the same at `0x5F507F..0x5F5085` and `0x5FC0E6..0x5FC0EC`; these preserve the field, they do not inspect it to select behavior.
- RW `0x5F461D` is the template-data xfer routine. At `0x5F4732..0x5F473B` it passes `&data[+0x83]` to the Xfer bool slot (`vtable +0x90`). The later version checks at `0x5F4741` and `0x5F4762` gate other fields (`+0x40`, `+0x98`, `+0x84`), not a maximum-height action.

## Runtime closure

The factory at RW `0x5F526A` returns an empty result for a null template; otherwise it increments the system ID and calls RW `0x5F477F`. That allocator creates a `0x1DC`-byte `ParticleSystem` and calls its constructor at RW `0x5FBE28`; the constructor installs vtable RW `0xBF7B48` and copies the template's system-info bytes, including the `+0x83` byte. The runtime vtable's update and particle-creation slots are RW `0x5FA2CE` and `0x5FC39A`.

I read the ground/position path at RW `0x5F4C6B`, including its calls to `0x5F4C02` and `0x5F4B88`. Its branches handle missing volume and particle transforms; none tests the copied `+0x83` byte. A `rw.py xref 0x83` review of the particle-system code range finds the field-table entry, the constructor, the copy operations and the xfer/write paths above, but no runtime condition or height-selection read. Thus the executable does not establish whether some hypothetical “maximum” would mean terrain, water, a bridge, or an object surface: that behavior is absent, not merely ambiguous.

## Donor and current port cross-check

DONOR BFME2 1.06 `git grep` finds `m_useMaximumHeight` only in the `ParticleSystemTemplate` member layout and parse table (`ParticleSystemTemplate.cpp:116,169`) and in `ParticleSystemTemplateWriteINI.cpp:100,128`. The donor has no gameplay reader for it. This corroborates, but does not replace, the RotWK disassembly above.

On the port's current `main`, `engine/src/GameClient/FXParticleSystem.cpp:683` parses the field and `FXParticleSystem.h:450` stores it at `+0x83`; executable code references show no renderer or simulation consumer. Two comments conflict: `ParticleSys.h:16` says the renderer consumes `UseMaximumHeight`, while `ParticleDraw.h:14` says it never reaches a draw module. Neither comment is an executable reader or overrides the RotWK disassembly. The Ennorath Balrog definitions therefore cannot supply a retail expected height for this flag.

## Tests and remaining question

No `PORT-111*` test is appropriate until a RotWK reader is demonstrated; a synthetic test that asserts terrain/water/bridge selection would encode an unsupported expectation. No existing code or test expectation was changed. See S-7287 in `docs/STOPS.md`.

**Осталось:** владелец должен решить, закрыть ли #271 как поле без runtime-эффекта в RotWK 2.01, или дать новый адрес/источник, подтверждающий отдельный runtime reader.
