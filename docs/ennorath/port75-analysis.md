# Port 75 · issue #207 — BannerCarrierUpdate: MorphCondition and BannerMorphFX

## Where the behaviour lives in the binary

The BannerCarrierUpdate translation unit of the byte-matched Open-BFME-2 decomp
(`Code/GameEngine/Source/GameLogic/Object/Update/BannerCarrierUpdate.cpp`) sits at
BFME2 0x0049xxxx; the port already carries six RW addresses of this TU that land
exactly 0x4037C0 above them:

| BFME2 (decomp, DONOR) | + 0x4037C0 | RW address the port already cites |
|---|---|---|
| update ctor 0x00496A98 | 0x89A258 | ctor RW 0x89A258 (BannerCarrierUpdate.h) |
| `BannerCarrierUpdate::update` 0x00497524 | 0x89ACE4 | update RW 0x89ACE4 |
| MorphCondition parser 0x0049714F | 0x89A90F | parser RW 0x89A90F (field-tables.json) |
| ExpLevelDraw parser 0x004972A9 | 0x89AA69 | parser RW 0x89AA69 |
| allow/upgrade gate `Rva00496B48` 0x00496B48 | 0x89A308 | RW 0x89A308 (AllowBannerSpawnUpgrade) |
| member spawn 0x00496BD2 | 0x89A392 | spawn RW 0x89A392 |

So the morph step `BannerCarrierUpdate::rva00496E05` (BFME2 0x00496E05) is
**RW 0x89A5C5** (derived, INFERENCE by that constant shift), its set lookup
`Rva00496D43` (BFME2 0x00496D43) is RW 0x89A503 and the locomotor lookup
`BannerCarrierUpdateModuleData::rva00496DA3` (BFME2 0x00496DA3) is RW 0x89A563.

## What the morph step does (DONOR BFME2 0x00496E05, byte order read)

`morph(entry, playFX)`:

1. `setFlags = lookup(m_data + 0x18, entry->name)` — the MorphCondition vector
   (`Rva00496D43`, BFME2 0x00496D43) is a list of `{AsciiString unitType (+0);
   19-word model-condition set (+8); AsciiString locomotor (+0x54)}`; a miss
   yields an **all-zero set** (`new (out) Rva0028F59A(0, 0xb8)`), not "skip".
2. clear-mask = the function-static mask built once from the 81-word table at
   BFME2 .rdata 0x00C4F6E0: `214, 194..198, 184..188, 458..527`.
   In RW's model-condition name table (RW 0xD9FAD8, 591 names, port
   `BitFlagNames.cpp`) those bits are `MOUNTED`, `PASSENGER_VARIATION_1..5`,
   `USER_1..USER_5`, `USER_6..USER_75` — i.e. exactly the "look" bits a banner
   carrier may take over.
3. `obj->clearAndSetModelConditionFlags(clear, set)` (RW 0x68D607).
4. `if (position->Length() > 0) obj->updateShroudNow()` (RW 0x68C7E9).
5. `if (m_data->getFX() && playFX) FXList::doFXObj(m_data + 0x30, obj, 0)`
   (RW 0x4B1B5A) — the FX plays whether or not an entry matched.
6. `loco = lookupLocomotor(m_data + 0x18, entry->name)`; if non-empty,
   `obj->getAI()->chooseLocomotorSet(index)` (AI slot 0x238).

## When it runs (DONOR BFME2 0x00473212 -> 0x00473197)

The only caller is the contain's accept path: `HordeContain::addMember`-shaped
`rva00473212` (BFME2 0x00473212, `HordeContainRva00473212.cpp`) tests
`accepts(object, carrierID, &fields->bannerNames, …)` — the object being taken in
is one of `BannerCarriersAllowed` — and then calls `rva00473197`
(`HordeContainBannerTransfer.cpp`): the carrier is put on the banner slot
(`getBannerCarrierIndexToUse(source, &type)`, BFME2 0x0046A521) and the morph is
applied with `type->name` — the template of the object it joins for, i.e. the
unit type of the horde it leads. The same call hands the horde's veterancy to
the carrier.

In the port the same accept action has two implementations
(`HordeContain::addToContain`, `HordeContain::acceptCreatedMember`), both of which
the task keeps as they are; the morph is one reaction behind both.

## Grammar (DONOR BFME2 0x0049714F, RW 0x89A90F)

```
MorphCondition = UnitType:<template> [Locomotor:<LocomotorSetName>] ModelState:"<names>"
```
`UnitType` is mandatory, `Locomotor` optional and before `ModelState`;
`ModelState` is mandatory and its quoted token is parsed as a model-condition
bit set (the quoted form is why the port kept it as a raw line).

## INFERENCE in the port

Retail keys the lookup by `getBannerCarrierIndexToUse(source, &type)` — the first
entry of the contain's banner index list when `source`'s template is not one of
them — which for every Ennorath horde is its main unit type. The port takes that
name directly from `HordeContain::getPayloadMemberTemplateName()` (single-entry
`InitialPayload`), the same accessor its `bannerMemberSlot` already resolves
`BannerCarrierPosition` with. A horde with several payload types keeps the
morph (the name is empty, nothing matches, the retail all-zero set applies), but
the looked-up type may differ from retail's when `source` is not the payload
template; that case is not in the mod's data.
