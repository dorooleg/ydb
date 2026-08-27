#pragma once

#include "blob.h"

#include <ydb/core/base/blobstorage.h>
#include <ydb/core/base/events.h>
#include <ydb/core/base/logoblob.h>
#include <ydb/core/tx/ctor_logger.h>

#include <ydb/library/actors/core/actor.h>
#include <ydb/library/actors/core/actorid.h>
#include <ydb/library/actors/core/event_local.h>
#include <ydb/library/actors/core/events.h>

#include <library/cpp/monlib/dynamic_counters/counters.h>
#include <util/generic/vector.h>
#include <util/system/unaligned_mem.h>

#include <atomic>
#include <memory>

namespace NKikimr::NBlobCache {

using NOlap::TBlobRange;
using NOlap::TUnifiedBlobId;

using TLogThis = TCtorLogger<NKikimrServices::BLOB_CACHE>;

struct TReadBlobRangeOptions {
    bool CacheAfterRead;
    bool IsBackgroud;
    bool WithDeadline = true;

    TString ToString() const {
        return TStringBuilder() << "cache: " << (ui32)CacheAfterRead << " background: " << (ui32)IsBackgroud
                                << " dedlined: " << (ui32)WithDeadline;
    }
};

struct TEvBlobCache {
    enum EEv {
        EvReadBlobRange = EventSpaceBegin(TKikimrEvents::ES_BLOB_CACHE),
        EvReadBlobRangeBatch,
        EvReadBlobRangeResult,
        EvCacheBlobRange,
        EvForgetBlob,

        EvEnd
    };

    static_assert(EvEnd < EventSpaceEnd(TKikimrEvents::ES_BLOB_CACHE), "Unexpected TEvBlobCache event range");

    struct TEvReadBlobRange: public NActors::TEventLocal<TEvReadBlobRange, EvReadBlobRange> {
        TBlobRange BlobRange;
        TReadBlobRangeOptions ReadOptions;

        explicit TEvReadBlobRange(const TBlobRange& blobRange, TReadBlobRangeOptions&& opts)
            : BlobRange(blobRange)
            , ReadOptions(std::move(opts))
        {
        }
    };

    // Read a batch of ranges from the same DS group
    // This is usefull to save IOPs when reading multiple columns from the same blob
    struct TEvReadBlobRangeBatch: public NActors::TEventLocal<TEvReadBlobRangeBatch, EvReadBlobRangeBatch> {
        std::vector<TBlobRange> BlobRanges;
        TReadBlobRangeOptions ReadOptions;

        explicit TEvReadBlobRangeBatch(std::vector<TBlobRange>&& blobRanges, TReadBlobRangeOptions&& opts)
            : BlobRanges(std::move(blobRanges))
            , ReadOptions(std::move(opts))
        {
        }
    };

    struct TEvReadBlobRangeResult: public NActors::TEventLocal<TEvReadBlobRangeResult, EvReadBlobRangeResult> {
        TBlobRange BlobRange;
        NKikimrProto::EReplyStatus Status;
        TString Data;
        TString DetailedError;
        const bool FromCache = false;
        const bool IsRetriable = false;
        const TInstant ConstructTime = Now();
        const TString DataSourceId;

        TEvReadBlobRangeResult(const TBlobRange& blobRange, NKikimrProto::EReplyStatus status, const TString& data, const TString& detailedError,
            const bool fromCache = false, const TString& dataSourceId = Default<TString>(), const bool isRetriable = false)
            : BlobRange(blobRange)
            , Status(status)
            , Data(data)
            , DetailedError(detailedError)
            , FromCache(fromCache)
            , IsRetriable(isRetriable)
            , DataSourceId(dataSourceId)
        {
        }
    };

    // Put a blob range data into cache. This helps to reduce number of reads from disks done by indexing, compactions
    // and queries that read recent data
    struct TEvCacheBlobRange: public NActors::TEventLocal<TEvCacheBlobRange, EvCacheBlobRange> {
        TBlobRange BlobRange;
        TString Data;
        bool Sticky = true;

        TEvCacheBlobRange(const TBlobRange& blobRange, const TString& data, const bool sticky = true)
            : BlobRange(blobRange)
            , Data(data)
            , Sticky(sticky)
        {
        }
    };

    // Notify the cache that this blob will not be requested any more
    // (e.g. when it was deleted after indexing or compaction)
    struct TEvForgetBlob: public NActors::TEventLocal<TEvForgetBlob, EvForgetBlob> {
        TUnifiedBlobId BlobId;

        explicit TEvForgetBlob(const TUnifiedBlobId& blobId)
            : BlobId(blobId)
        {
        }
    };
};

constexpr ui32 BlobCacheShardCount = 16;
constexpr ui64 DefaultBlobCacheMaxBytes = 1000ull << 20;

class TBlobCacheSharedState: public TThrRefBase {
public:
    const ui32 ShardCount;
    std::atomic<i64> TotalMaxCacheDataSize;

    TBlobCacheSharedState(const ui32 shardCount, const i64 totalMaxCacheDataSize)
        : ShardCount(shardCount)
        , TotalMaxCacheDataSize(totalMaxCacheDataSize)
        , ShardCacheDataSize(std::make_unique<std::atomic<ui64>[]>(shardCount))
    {
        for (ui32 i = 0; i < shardCount; ++i) {
            ShardCacheDataSize[i].store(0, std::memory_order_relaxed);
        }
    }

    void SetShardCacheDataSize(const ui32 shard, const ui64 size) {
        ShardCacheDataSize[shard].store(size, std::memory_order_relaxed);
    }

    ui64 GetTotalCacheDataSize() const {
        ui64 total = 0;
        for (ui32 i = 0; i < ShardCount; ++i) {
            total += ShardCacheDataSize[i].load(std::memory_order_relaxed);
        }
        return total;
    }

private:
    std::unique_ptr<std::atomic<ui64>[]> ShardCacheDataSize;
};

inline TIntrusivePtr<TBlobCacheSharedState> MakeBlobCacheSharedState(const ui32 shardCount, const i64 totalMaxCacheDataSize) {
    return MakeIntrusive<TBlobCacheSharedState>(shardCount, totalMaxCacheDataSize);
}

inline ui32 BlobCacheShardIndex(const TUnifiedBlobId& blobId) {
    return static_cast<ui32>(blobId.Hash() % BlobCacheShardCount);
}

inline NActors::TActorId MakeBlobCacheServiceId(ui32 shardIndex = 0) {
    static_assert(TActorId::MaxServiceIDLength == 12, "Unexpected actor id length");
    char x[12] = { 'b', 'l', 'o', 'b', '_', 'c', 'a', 'c', 'h', 'e', 0, 0 };
    WriteUnaligned<ui16>(x + 10, static_cast<ui16>(shardIndex % BlobCacheShardCount));
    return TActorId(0, TStringBuf(x, 12));
}

inline NActors::TActorId MakeBlobCacheServiceId(const TUnifiedBlobId& blobId) {
    return MakeBlobCacheServiceId(BlobCacheShardIndex(blobId));
}

NActors::IActor* CreateBlobCache(const std::optional<ui64>& maxBytes, TIntrusivePtr<::NMonitoring::TDynamicCounters> counters);
NActors::IActor* CreateBlobCache(const std::optional<ui64>& maxBytes, TIntrusivePtr<::NMonitoring::TDynamicCounters> counters,
    ui32 shardIndex, ui32 shardCount, TIntrusivePtr<TBlobCacheSharedState> sharedState);

void SendReadBlobRangeBatch(std::vector<TBlobRange>&& blobRanges, TReadBlobRangeOptions opts);

// Explicitly add and remove data from cache. This is usefull for newly written data that is likely to be read by
// indexing, compaction and user queries and for the data that has been compacted and will not be read again.
inline void AddRangeToCache(const TBlobRange& blobRange, const TString& data) {
    if (!TlsActivationContext) {
        return;
    }
    TlsActivationContext->Send(
        new IEventHandle(MakeBlobCacheServiceId(blobRange.BlobId), NActors::TActorId(), new TEvBlobCache::TEvCacheBlobRange(blobRange, data)));
}

inline void ForgetBlob(const TUnifiedBlobId& blobId) {
    if (!TlsActivationContext) {
        return;
    }
    TlsActivationContext->Send(new IEventHandle(MakeBlobCacheServiceId(blobId), NActors::TActorId(), new TEvBlobCache::TEvForgetBlob(blobId)));
}

}   // namespace NKikimr::NBlobCache
