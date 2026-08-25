#include <ydb/core/testlib/basics/appdata.h>
#include <ydb/core/testlib/basics/runtime.h>
#include <ydb/core/testlib/tablet_helpers.h>
#include <ydb/core/tx/columnshard/blob_cache.h>

#include <library/cpp/testing/unittest/registar.h>

namespace NKikimr::NBlobCache {

namespace {

using namespace NActors;

TBlobRange MakeBlobRange(const ui32 cookie, const ui32 blobSize, const ui32 offset = 0, const ui32 rangeSize = 0) {
    const ui32 size = rangeSize ? rangeSize : blobSize - offset;
    TLogoBlobID blobId(1, 1, 1, 0, blobSize, cookie);
    return TBlobRange(TUnifiedBlobId(1, blobId), offset, size);
}

struct TCacheEnv {
    TTestBasicRuntime Runtime;
    TActorId Cache;
    TActorId Sender;
    TIntrusivePtr<::NMonitoring::TDynamicCounters> Counters;

    explicit TCacheEnv(const ui64 maxBytes) {
        TAppPrepare app;
        SetupTabletServices(Runtime, &app, true);
        Counters = MakeIntrusive<::NMonitoring::TDynamicCounters>();
        Cache = Runtime.Register(CreateBlobCache(maxBytes, Counters));
        Runtime.EnableScheduleForActor(Cache);
        Sender = Runtime.AllocateEdgeActor();
        Runtime.DispatchEvents(TDispatchOptions(), TDuration::MilliSeconds(1));
    }

    void AddToCache(const TBlobRange& range, const TString& data, const bool sticky = true) {
        Runtime.Send(new IEventHandle(Cache, Sender, new TEvBlobCache::TEvCacheBlobRange(range, data, sticky)), 0, true);
        Runtime.DispatchEvents(TDispatchOptions(), TDuration::MilliSeconds(1));
    }

    TEvBlobCache::TEvReadBlobRangeResult* Read(const TBlobRange& range, TAutoPtr<IEventHandle>& handle, const TDuration timeout = TDuration::Seconds(1)) {
        TReadBlobRangeOptions opts{ .CacheAfterRead = false, .IsBackgroud = false };
        Runtime.Send(new IEventHandle(Cache, Sender, new TEvBlobCache::TEvReadBlobRange(range, std::move(opts))), 0, true);
        return Runtime.GrabEdgeEvent<TEvBlobCache::TEvReadBlobRangeResult>(handle, timeout);
    }

    i64 Counter(const TString& name, const bool derivative = true) const {
        return Counters->GetCounter(name, derivative)->Val();
    }
};

}   // namespace

Y_UNIT_TEST_SUITE(TBlobCacheWriteProtect) {
    Y_UNIT_TEST(WriteInsertThenExactHit) {
        TCacheEnv env(1ull << 20);
        const auto range = MakeBlobRange(1, 16);
        const TString data(16, 'A');
        env.AddToCache(range, data);

        TAutoPtr<IEventHandle> handle;
        auto* result = env.Read(range, handle);
        UNIT_ASSERT(result);
        UNIT_ASSERT_VALUES_EQUAL(result->Status, NKikimrProto::OK);
        UNIT_ASSERT(result->FromCache);
        UNIT_ASSERT_VALUES_EQUAL(result->Data, data);
        UNIT_ASSERT_VALUES_EQUAL(env.Counter("StickyHits"), 1);
        UNIT_ASSERT_VALUES_EQUAL(env.Counter("StickyHitsBytes"), 16);
        UNIT_ASSERT_VALUES_EQUAL(env.Counter("Hits"), 1);
    }

    Y_UNIT_TEST(CoveringSubrangeHit) {
        TCacheEnv env(1ull << 20);
        const auto full = MakeBlobRange(2, 32);
        TString data;
        data.reserve(32);
        for (ui32 i = 0; i < 32; ++i) {
            data += static_cast<char>('a' + (i % 26));
        }
        env.AddToCache(full, data);

        const auto sub = MakeBlobRange(2, 32, 8, 10);
        TAutoPtr<IEventHandle> handle;
        auto* result = env.Read(sub, handle);
        UNIT_ASSERT(result);
        UNIT_ASSERT_VALUES_EQUAL(result->Status, NKikimrProto::OK);
        UNIT_ASSERT(result->FromCache);
        UNIT_ASSERT_VALUES_EQUAL(result->Data, data.substr(8, 10));
        UNIT_ASSERT_VALUES_EQUAL(env.Counter("StickyHits"), 1);
        UNIT_ASSERT_VALUES_EQUAL(env.Counter("StickyHitsBytes"), 10);
    }

    Y_UNIT_TEST(EvictNonStickyBeforeSticky) {
        TCacheEnv env(100);
        const auto regularRange = MakeBlobRange(3, 60);
        const auto stickyRange = MakeBlobRange(4, 60);
        env.AddToCache(regularRange, TString(60, 'C'), false);
        env.AddToCache(stickyRange, TString(60, 'D'), true);

        TAutoPtr<IEventHandle> stickyHandle;
        auto* stickyResult = env.Read(stickyRange, stickyHandle);
        UNIT_ASSERT(stickyResult);
        UNIT_ASSERT(stickyResult->FromCache);
        UNIT_ASSERT_VALUES_EQUAL(stickyResult->Data, TString(60, 'D'));

        TAutoPtr<IEventHandle> regularHandle;
        auto* regularResult = env.Read(regularRange, regularHandle, TDuration::MilliSeconds(50));
        UNIT_ASSERT(!regularResult);
        UNIT_ASSERT_VALUES_EQUAL(env.Counter("StickyHits"), 1);
        UNIT_ASSERT_VALUES_EQUAL(env.Counter("StickyEvictions"), 0);
        UNIT_ASSERT_VALUES_EQUAL(env.Counter("Evictions"), 1);
    }

    Y_UNIT_TEST(EvictOldestStickyWhenOnlyStickyRemain) {
        TCacheEnv env(100);
        const auto first = MakeBlobRange(5, 60);
        const auto second = MakeBlobRange(6, 60);
        env.AddToCache(first, TString(60, 'E'), true);
        env.AddToCache(second, TString(60, 'F'), true);

        TAutoPtr<IEventHandle> secondHandle;
        auto* secondResult = env.Read(second, secondHandle);
        UNIT_ASSERT(secondResult);
        UNIT_ASSERT(secondResult->FromCache);
        UNIT_ASSERT_VALUES_EQUAL(secondResult->Data, TString(60, 'F'));

        TAutoPtr<IEventHandle> firstHandle;
        auto* firstResult = env.Read(first, firstHandle, TDuration::MilliSeconds(50));
        UNIT_ASSERT(!firstResult);
        UNIT_ASSERT_VALUES_EQUAL(env.Counter("StickyHits"), 1);
        UNIT_ASSERT_VALUES_EQUAL(env.Counter("StickyEvictions"), 1);
        UNIT_ASSERT_VALUES_EQUAL(env.Counter("StickyEvictedBytes"), 60);
    }

    Y_UNIT_TEST(RegularHitDoesNotCountAsSticky) {
        TCacheEnv env(1ull << 20);
        const auto range = MakeBlobRange(8, 16);
        env.AddToCache(range, TString(16, 'H'), false);

        TAutoPtr<IEventHandle> handle;
        auto* result = env.Read(range, handle);
        UNIT_ASSERT(result);
        UNIT_ASSERT(result->FromCache);
        UNIT_ASSERT_VALUES_EQUAL(env.Counter("Hits"), 1);
        UNIT_ASSERT_VALUES_EQUAL(env.Counter("StickyHits"), 0);
        UNIT_ASSERT_VALUES_EQUAL(env.Counter("StickyHitsBytes"), 0);
    }

    Y_UNIT_TEST(ForgetBlobDropsStickyAndCovering) {
        TCacheEnv env(1ull << 20);
        const auto full = MakeBlobRange(7, 32);
        env.AddToCache(full, TString(32, 'G'), true);

        env.Runtime.Send(new IEventHandle(env.Cache, env.Sender, new TEvBlobCache::TEvForgetBlob(full.BlobId)), 0, true);
        env.Runtime.DispatchEvents(TDispatchOptions(), TDuration::MilliSeconds(1));

        TAutoPtr<IEventHandle> exactHandle;
        UNIT_ASSERT(!env.Read(full, exactHandle, TDuration::MilliSeconds(50)));

        TAutoPtr<IEventHandle> subHandle;
        UNIT_ASSERT(!env.Read(MakeBlobRange(7, 32, 4, 8), subHandle, TDuration::MilliSeconds(50)));
    }
}

}   // namespace NKikimr::NBlobCache
