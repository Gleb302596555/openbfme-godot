# PORT-140 (issue #342, P-11 "the creep troll throws rocks it does not have and does no damage"): synthetic-INI tests for
# openbfme_tests. The engine change is in GameLogic/Combat/ObjectWeapons.cpp (the live WeaponHost), so no new source file.
#
# RW: Weapon::getRemainingAmmo RW 0x6CD298 asks the object's container ([Object+0x258], RW 0x68B678) for the number of items its
# ProjectileFilterInContainer (+0x120) accepts — `mov eax,[ecx]; push &filter; call [eax+0x114]` — and OpenContain's slot 0x114
# RW 0x865CE3 counts the contained list (`[edi+0x38]` unfiltered, one `inc` per item ObjectFilter::testTemplate RW 0x7640C1 ->
# RW 0x763543 accepts). Weapon::getStatus RW 0x6CD142 consults it before it answers READY_TO_FIRE, so a weapon that throws what it
# carries is ready only while the container holds such an item. Weapon::ownerHasContainAmmo RW 0x6CD11F -> RW 0x68B634 ->
# RW 0x8F1149 (`mov eax,[ecx+0x24]`) is the same question through the template's own filter.
set(OPENBFME_PORT140_TESTS
    tests/test_port_140.cpp)
