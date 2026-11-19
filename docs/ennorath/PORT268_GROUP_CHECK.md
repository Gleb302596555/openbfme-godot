# Port 268 — preliminary manual grouping check

The following is a check of an **intermediate** ranking, not a claim that the
final grouping is correct. Commands use `python tools/mod/rw.py
workspace/RotWK/game.dat dis <address> 12`. The binary is not included.

1. `ActiveBody`, S-1712/S-681/S-854: `RW 0x8B38A2` is an entry and
   `0x8B38B0` is inside it. The first two stops describe different damage
   paths, while S-854 concerns respawn. A shared class mention is not enough
   evidence of one task: split and examine how class mentions are attributed.
2. `ReplaceSelfUpgrade`, S-081/S-1986/S-233: `RW 0x5E586F` is a function
   entry, but it is locomotor numerics, whereas S-1986 is wall replacement.
   The reference-based split incorrectly leaves unrelated mechanisms together.
3. `RW 0x4BE587`, S-091/S-1580/S-5920: the function starts with an SEH
   prologue at `0x4BE587`; S-1580 and S-5920 cite interior instructions.
   Shared code is real, but parsing Lua scripts, animation timing, and a
   presentation adaptation still require separate scope review.
4. `SpecialPowerModule`, S-2006/S-2008: `RW 0x86C4EA` is a prologue and
   appears in both records, but the two sweep failures do not thereby prove a
   common mechanism. Naming the group after a mentioned class is too strong.
5. `SpecialAbilityUpdate`, S-7224/S-7360: `RW 0x85260A` starts with stack
   allocation and accesses the ability's sound state; S-7224 cites `0x85261D`.
   S-7360 describes building EVA. A shared function must not be conflated with
   proof that all these event paths are one implementation task.

Also corrected two false interior starts: `0x8A3F72` lies inside the map start
`0x8A3DD5`, and `0x4C36F8` lies inside `0x4C34A2` (both verified with
`rw.py dis`). The resolver now prefers a reachable map start over an
unverified interior candidate. Candidate entry detection still needs review;
only the cached whole-function inventory can supply its actual blocks.

## Current ranking, second pass

Commands: `python tools/mod/rw.py workspace/RotWK/game.dat dis <VA> 14`.

1. `RW 0x5D893C`, S-185/S-322/S-636: the SEH entry and ARMOR modifier
   reads `0x5D8A64`/`0x5D8AF1` really concern AdjustDamage. They are a
   defensible shared-function candidate, but S-185 and S-636 specifically
   concern the community-patched section, so an unpatched binary is needed
   for a vanilla conclusion. This candidate is not a proof of a closure.
2. `RW 0x4BE587`, S-091/S-1580/S-5920: the SEH entry and its shared code
   are real. The Lua protocol, animation timing and owner presentation repair
   are distinct branches, so a single implementation issue is not yet justified.
3. S-017/S-123: `RW 0xB5F9F0` begins with an x87 load and is quoted only in
   S-123; S-017 is connected by reference, not a demonstrated common
   function. Labelled linked stops rather than the misleading shared function.
4. S-041/S-120: `RW 0xB5B320` indexes a type-name table and returns; it is
   cited by S-041. S-120 concerns the Lua fork generally and refers to S-041.
   Labelled linked stops rather than a shared-function claim.
5. S-081/S-233: `RW 0x42F4E0` is `fld; fcos; ret`; S-081 cites the helper,
   while S-233 refers to the numerical bounds. This is a dependency candidate,
   not proof that all its numerical behavior is fixed by porting `fcos`.

Corrections after the first pass: class edges now require the primary area or
first class in the gap, not a class mentioned in a distant caller; a split no
longer joins two independently placed mechanisms merely due to a reference;
common utility functions and explicitly historical fixes do not form function
edges. All references and citations remain visible. Unknown function status is
included as unverified work in the ranking denominator, never claimed as
`none`. These safeguards reduce but do not eliminate false positives.

## Final ranking after splitting the reviewed dispatcher

The top four are the AdjustDamage group, the S-017/S-123 numeric dependency,
the S-041/S-120 Lua dependency, and the S-081/S-233 numeric dependency;
the instructions and qualifications for each are recorded above. The fifth
is S-1122/S-723: `rw.py dis 0x6AF598 18` shows the SEH prologue of the
asset-transfer function and its player-state access at `0x6AF5AD`. S-723
explicitly names S-1122 as an open disconnect point: the reference link is
real, not a claim that both descriptions cite this function. The first four
likewise remain candidates, not proven one-task closures.

The formerly second-ranked S-091/S-1580/S-5920 group is split: the reviewed
`0x4BE587` is a large dispatcher, not by itself evidence of a single task.
Another incorrect edge joined S-076 to S-091 through `RW 0x42D400`; the first
stop concerns unterminated INI bodies, whereas the second quotes already
stored script text as background for its host/API gap. The historical-citation
filter now removes that edge while retaining the citation. Separately, the
resolver missed S-126's `RW 0x734EF4`: `rw.py dis 0x734E94 18` shows an SEH
entry immediately after `ret` at `0x734E93`. Post-return SEH prologues are now
candidate starts, and the reachable-code check maps the instruction to
`RW 0x734E94`. This is a heuristic, not a replacement for a block inventory.
