# PORT-335: the last uncited own functions of AttributeModifierPoolUpdate, ObjectCreationUpgrade, ActivateModuleSpecialPower, RousingSpeechUpdate,
# BloodthirstyUpdate and ThreatFinderUpdate (fork issue 890). The lane's engine changes are in files other lists already carry
# (Common/System/SubsystemLegend, Common/SubsystemLegend.h, GameLogic/Object/Object, GameLogic/Module/BehaviorModule.h,
# GameLogic/Module/BloodthirstyUpdate, GameLogic/Object/AttributeModifierPool, GameLogic/Module/ObjectCreationUpgrade,
# GameLogic/Module/HeroAbilityModules, GameClient/UnitVoiceResponse).
set(OPENBFME_PORT335_TESTS
    tests/test_port_335.cpp
)
