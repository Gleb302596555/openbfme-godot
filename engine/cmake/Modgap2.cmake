# MODGAP-2: Ennorath module tests and runtime sources owned by agent B.
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/FlingPassengerSpecialAbilityUpdate.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/GiveUpgradeUpdate.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/LocomotorSetUpgrade.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/ReplaceObjectUpdate.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/ShipSlowDeathBehavior.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/StoreObjectsSpecialPower.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/SplitHordeSpecialPower.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/TeleportToCasterSpecialPower.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/ClearanceTestingSlowDeathGeometry.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/SpecialEnemySenseUpdate.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/OathbreakersFadeAwayBehavior.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/AODCrushCollide.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/Common/FunctionCurve.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/DestroyEnvironmentUpdate.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/PartTheHeavensUpdate.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/OilSpillUpdate.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DTornadoDraw.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/TooltipUpgrade.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/AnnounceBirthAndDeathBehavior.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/ProjectileStreamUpdate.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/StrafeAreaUpdate.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DProjectileStreamDraw.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DLightDraw.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/AnimalAIUpdate.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/HordeWorkerAIUpdate.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/RainOfFireUpdate.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/WallUpgradeUpdate.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/RunOffMapBehavior.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/DetachableRiderUpdate.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/BaseUpgrade.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/DamageFieldUpdate.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/CivilianSpawnUpdate.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/CivilianSpawnCollide.cpp)
set(OPENBFME_MODGAP2_TESTS
    tests/test_modgap2_bored.cpp
    tests/test_modgap2_aod_crush.cpp
    tests/test_modgap2_clearance.cpp
    tests/test_modgap2_clearance_geometry.cpp
    tests/test_modgap2_special_enemy.cpp
    tests/test_modgap2_oath.cpp
    tests/test_modgap2_fling.cpp
    tests/test_modgap2_give_upgrade.cpp
    tests/test_modgap2_audio_loop.cpp
    tests/test_ennorath_scenarios.cpp
    tests/test_modgap2_locomotor_upgrade.cpp
    tests/test_modgap2_replace.cpp
    tests/test_modgap2_ring_chain.cpp
    tests/test_modgap2_ship_death.cpp
    tests/test_modgap2_split.cpp
    tests/test_modgap2_store_teleport.cpp
    tests/test_modgap2_store_objects.cpp
    tests/test_modgap2_troll_animation.cpp
    tests/test_modgap2_builder_heading.cpp
    tests/test_modgap2_env_heavens_radar.cpp
    tests/test_modgap2_oilspill.cpp
    tests/test_modgap2_tornado.cpp
    tests/test_modgap2_tooltip_upgrade.cpp
    tests/test_modgap2_announce.cpp
    tests/test_modgap2_stream_strafe_light.cpp
    tests/test_modgap2_eva_spawn.cpp
    tests/test_modgap2_ai_scripts.cpp
)
