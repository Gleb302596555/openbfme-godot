# RW 0x86AAAE / 0x86AD95 - TransportContain passengers fade on enter and on exit

Port 97, issue #245. `FadePassengerOnEnter` (RW 0xC5A9B0), `FadePassengerOnExit`,
`EnterFadeTime` (RW 0xC5A98C), `ExitFadeTime`, `FadeReverse` and `FadeFilter` are the
fields; both fades live at the end of `TransportContain::onContaining` (RW 0x86A943) and
`TransportContain::onRemoving` (RW 0x86ABDC).

The drawable the two blocks fade is the rider's: the enter block reads it once, at
RW 0x86A9C9 .. 0x86A9D0, through RW 0x70E013 (`Object::getDrawable`, `Object + 0x84`),
into the local `[ebp + 0xC]`; the exit block keeps the same pointer in `[ebp - 0x10]`.
Both are client-only: they only touch `Drawable::fadeIn` / `Drawable::fadeOut`
(RW 0x670AA2 / RW 0x670A50), already ported by lane PROJ-2 in `GameClient/DrawableFade.cpp`.

## On enter - RW 0x86AAC2 .. 0x86ABCB (end of onContaining, after the rider is hidden)

| address | what it reads / does |
| --- | --- |
| RW 0x86AAC2 | `cmp byte [data + 0x16C], 0` - `FadePassengerOnEnter`; zero -> 0x86ABCB, no fade |
| RW 0x86AACF | `cmp dword [ebp + 0xC], 0` - the rider's drawable; null -> 0x86ABCB |
| RW 0x86AAD9 .. 0x86AAEC | `EnterFadeTime` (data + 0x170) against `[0xC1B594]`; `ucomiss` / `lahf` / `test ah,0x44` / `jnp` skips the fade only when the two compare equal (`EnterFadeTime == 0`) |
| RW 0x86AAF2 .. 0x86AB02 | `push 0; push rider; ecx = data + 0x168` -> RW 0x7640C1 `ObjectFilter::accepts`; false -> 0x86ABCB |
| RW 0x86AB08 .. 0x86AB12 | `eax = [rider + 4]` (the template), `test byte [eax + 0x115], 0x20` - `KINDOF_HORDE` (port `isKindOf(109)`, RW 0x79F22D); not a horde -> the single path at 0x86ABA1 |
| RW 0x86AB18 .. 0x86AB21 | RW 0x68C866 - the rider's contain (`Object + 0x258`, vslot 0x7C); null -> the single path |
| RW 0x86AB23 .. 0x86AB42 | the member list at `[ebp + 0xC]` (ctor RW 0x5FFE1E) filled by the contain's vslot 0x10C |
| RW 0x86AB48 .. 0x86AB91 | per member: `ecx = [esi + 8]` (the object) -> RW 0x70E013 (its drawable); frames = `ftol(0.03 * EnterFadeTime)` (0x03 is `[0xD9F624]`, RW 0xA3CFA4 is ftol); `FadeReverse` (data + 0x178) == 0 -> RW 0x670AA2 `fadeIn(frames)`, != 0 -> RW 0x670A50 `fadeOut(frames)`; list dtor RW 0x5FFE47 at 0x86AB97 |
| RW 0x86ABA1 .. 0x86ABCB | the single path: the same frames and the same `FadeReverse` switch on the rider's own drawable (`ecx = [ebp + 0xC]`) |

## On exit - RW 0x86ADBD .. 0x86AEA1 (end of onRemoving)

The same four guards with the exit fields: `FadePassengerOnExit` (data + 0x16D) at
RW 0x86ADBD, the rider's drawable (`[ebp - 0x10]`) at RW 0x86ADCC, `ExitFadeTime`
(data + 0x174) at RW 0x86ADD5 .. 0x86ADE8, `FadeFilter` -> RW 0x7640C1 at
RW 0x86ADEE .. 0x86ADFD. The horde test (RW 0x86AE03), the contain (RW 0x68C866 at
RW 0x86AE0F) and the member walk (vslot 0x10C at RW 0x86AE38, RW 0x70E013 at
RW 0x86AE4E) are the same, and so is `ftol(0.03 * ExitFadeTime)`.

One difference: the exit block has **no** `FadeReverse` test. Every exit fade, the
horde members' (RW 0x86AE6D) and the single rider's, is `RW 0x670AA2 fadeIn(frames)`.

## Frames

`ftol(0.03 * time)`: `fld [0xD9F624]` (0.03) / `fmul [data + 0x170 | 0x174]` / `call
0xA3CFA4` (ftol, truncation). `EnterFadeTime 1000` -> 30 client frames; the mumak's
`EnterFadeTime 6000` -> 180 frames, 6 seconds at the client's 30 Hz.

## Donor

DONOR BFME2: `Code/GameEngine/Source/GameLogic/Object/Contain/TransportContain.cpp`
`Rva00467AB8::onRemoving` fades the rider (or its contain's members) with
`fadeIn(BfmeClientFramesPerMillisecond * module->m_fadeTime)` - the same 0.03 and the
same `fadeIn` of the exit block, but from the base module's `fade` / `fadeTime`, not
from `FadePassengerOnExit`. The donor has no enter fade at all: `m_fadePassengerOnEnter`
appears in its module data only in the field table, the constructor (line 1506) and the
destructor. RotWK added the whole enter block and the `FadeReverse` switch.
