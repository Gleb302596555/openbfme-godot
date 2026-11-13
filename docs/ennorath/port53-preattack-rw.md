# Port 53: PreAttackFX investigation (RotWK 2.01)

Addresses below were read with `tools/mod/rw.py ... dis`, except for the vtable
entry, read as a pointer from the PE image. No retail data is reproduced here.

* Parser RW 0x6C9CDC calls the FX-list parser at 0x6C9CEF and writes its
  result four times at 0x6C9CFA–0x6C9D00. Both runtime readers below use the
  first entry at template +0xB8 directly, not an indexed veterancy entry.
* Weapon::preFireWeapon RW 0x6CE95D draws jitter and computes the delay; the
  signed `jle` at 0x6CE980 returns without the FX when the delay is <= 0.
  Otherwise it sets the status, bonuses and timers, calls the nugget hook at
  0x6CEA12, reads speed +0x68 and FX +0xB8 at 0x6CEA17–0x6CEA1A, then uses
  the shooter's drawable position, matrix and position at 0x6CEA25–0x6CEA56.
  RW 0x6CEA58 calls the positional FX wrapper 0x494615, which checks the FX
  pointer and culling at 0x494618–0x494628 and forwards all arguments to
  0x5E21D8 at 0x49462A–0x49463D.
* WeaponFireSpecialAbilityUpdate factory RW 0x64D7F4 allocates 0x8C and calls
  constructor 0x895D98 at 0x64D81E. That constructor installs vtable 0xC64818
  at 0x895DC6 and resolves/creates/loads the special weapon into +0x88 at
  0x895DE1–0x895E22. The PE pointer at 0xC64870 (vtable +0x58) is 0x895ECE;
  the update invokes slot +0x58 at RW 0x8551FA–0x8551FC following its gate at
  0x8551EF–0x8551F8. This connects the reader to the class and caller.
* RW 0x895ECE calls the base unpacking and sets the appropriate special-weapon
  condition unless the skip-continue/trigger-count gate takes 0x895EF4. Both
  branches reach 0x895FC4: byte +0xE1 must be set, then weapon +0x88,
  template +4 and template FX +0xB8 must all be non-null (0x895FC7–0x895FE9).
  Speed +0x68 and the owner's drawable position/matrix/position are passed to
  0x494615 at 0x895FEB–0x896026. It is the owner's drawable, not the target.
* The port already has Local 22's `FXEventLog::drawablePositionEvent`, but
  neither reader emits it. The special weapon is made lazily in
  `WeaponFireSpecialAbilityUpdate::specialWeapon`, and its creation is hashed
  and saved; moving that creation to unpacking only for this FX would change
  simulation state. The constructor/host lifetime mismatch needs careful
  treatment rather than an FX-only workaround.
