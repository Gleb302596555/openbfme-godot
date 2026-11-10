# PORT-68 (issue #182, Weapon/FXTrigger and OverrideVoiceAttackSound / OverrideVoiceEnterStateAttackSound):
# synthetic-INI tests for openbfme_tests. RW 0x73AB45 / 0x73ACEF (the weapon voice override pair at +0xD0 / +0xD8),
# RW 0x8DE0F4 .. 0x8DE150 (UnitVoiceResponse::attackVoices reads the current weapon's pair through RW 0x68B58C),
# RW 0x692223 (`ObjectKnockback::apply` maps the projectile name through RW 0xDA08FC and stores it via RW 0x68B56A),
# RW 0x6968BC (the shockwave handler clears Drawable + 0x360 at RW 0x6968D7 and writes DamageInfo + 0x2C at RW 0x696E39)
# and RW 0x4C662D (the ParticleSysBone FXTrigger gate of RW 0x4C6514).
set(OPENBFME_PORT68_TESTS
    tests/test_port_68.cpp)
