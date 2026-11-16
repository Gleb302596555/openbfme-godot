# PORT-203 (issue #470): TunnelTracker/m_curNemesisID & m_nemesisTimestamp, exposeStealthUnits on removeFromContain,
# TunnelContain::ExitInterface (isExitBusy / exitObjectInAHurry), AITunnelNetworkGuardState (0x18) + AITNGuardMachine (5000..5005),
# aiGuardTunnelNetwork (AICMD 0x37), and script action TEAM_GUARD_IN_TUNNEL_NETWORK (326).

set(OPENBFME_PORT203_TESTS
    tests/test_port_203.cpp)

# simulation sources (sim audit manifest): RotWK's tunnel network guard state / machine (RW 0x754259 / 0x94C8D0)
list(APPEND OPENBFME_SIM_SOURCES
    src/GameLogic/AI/AITNGuard.cpp
)
