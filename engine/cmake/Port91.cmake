# PORT-91 (issue #239): W3DModelDraw/ShowShadowWhileContained — a contained unit keeps its shadow (RW 0x4C7848's gate; vslot 0x2C of the contain,
# RW 0x9188EB false everywhere but the HordeContain family's RW 0x86C737). The engine change is in files other lists already carry
# (GameLogic/Module/BehaviorModule.h, GameLogic/Object/Contain/HordeContainRuntime.{h,cpp}, GameClient/LogicSnapshot.{h,cpp}, GameClient/ObjectShadow.{h,cpp},
# GodotDevice/GodotGameWorld.{h,cpp}, GodotDevice/GodotInGameHud.cpp).

set(OPENBFME_PORT91_TESTS
    tests/test_port_91.cpp
)
