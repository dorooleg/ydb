UNITTEST_FOR(ydb/core/tx/columnshard)

SIZE(SMALL)

SRCS(
    ut_blob_cache.cpp
)

PEERDIR(
    library/cpp/testing/unittest
    ydb/core/testlib/default
    ydb/core/tx/columnshard
)

YQL_LAST_ABI_VERSION()

END()
