# PORT-51: Object/InstanceScaleFuzziness — the row is parsed, RotWK 2.01 never reads it (issue #155). The tests pin that the instance
# scale is Scale alone; no engine source is added by this lane (docs/ennorath/PORT51_RW_ANALYSIS.md).

set(OPENBFME_PORT51_TESTS
    tests/test_port_51.cpp
)
