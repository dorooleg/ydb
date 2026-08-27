#include "blob_cache.h"
#include "columnshard.h"

#include <ydb/core/base/appdata.h>

#include <ydb/core/base/appdata.h>
#include <ydb/core/base/blobstorage.h>
#include <ydb/core/base/memory_controller_iface.h>
#include <ydb/core/base/tablet_pipe.h>

#include <ydb/library/actors/core/actor.h>
#include <ydb/library/actors/core/hfunc.h>

#include <library/cpp/cache/cache.h>
#include <util/generic/hash.h>
#include <util/generic/set.h>
#include <util/string/cast.h>
#include <util/string/vector.h>
#include <optional>

#include <array>
#include <algorithm>
#include <memory>
#include <tuple>

#define YDB_LOG_THIS_FILE_COMPONENT NKikimrServices::BLOB_CACHE

namespace NKikimr::NBlobCache {
namespace {

using namespace NActors;

class TBlobCache: public TActorBootstrapped<TBlobCache> {
private:
    struct TReadInfo {
        /// List of readers.
        TList<TActorId> Waiting;
        /// Put in cache after read.
        bool Cache{ false };
    };

    struct TReadItem: public TReadBlobRangeOptions {
        enum class EReadVariant {
            FAST = 0,
            DEFAULT,
            DEFAULT_NO_DEADLINE,
        };

        TBlobRange BlobRange;

        TReadItem(const TReadBlobRangeOptions& opts, const TBlobRange& blobRange)
            : TReadBlobRangeOptions(opts)
            , BlobRange(blobRange)
        {
            Y_ABORT_UNLESS(blobRange.BlobId.IsValid());
        }

        bool PromoteInCache() const {
            return CacheAfterRead;
        }

        static NKikimrBlobStorage::EGetHandleClass ReadClass(EReadVariant readVar) {
            return (readVar == EReadVariant::FAST) ? NKikimrBlobStorage::FastRead : NKikimrBlobStorage::AsyncRead;
        }

        EReadVariant ReadVariant() const {
            return IsBackgroud ? (WithDeadline ? EReadVariant::DEFAULT : EReadVariant::DEFAULT_NO_DEADLINE) : EReadVariant::FAST;
        }

        // Blobs with same tagret can be read in a single request
        // (e.g. DS blobs from the same tablet residing on the same DS group, or 2 small blobs from the same tablet)
        std::tuple<ui64, ui32, EReadVariant> BlobSource() const {
            const TUnifiedBlobId& blobId = BlobRange.BlobId;
            Y_ABORT_UNLESS(blobId.IsValid());
            return { blobId.GetTabletId(), blobId.GetDsGroup(), ReadVariant() };
        }
    };

    /// Hash TBlobRange by BlobId only.
    struct BlobRangeHash {
        size_t operator()(const TBlobRange& range) const {
            return range.BlobId.Hash();
        }

        size_t operator()(const TUnifiedBlobId& id) const {
            return id.Hash();
        }
    };

    /// Compares TBlobRange by BlobId only.
    struct BlobRangeEqual {
        bool operator()(const TBlobRange& a, const TBlobRange& b) const {
            return a.BlobId == b.BlobId;
        }

        bool operator()(const TBlobRange& a, const TUnifiedBlobId& id) const {
            return a.BlobId == id;
        }
    };

    static constexpr i64 MAX_IN_FLIGHT_BYTES = 250ll << 20;
    static constexpr i64 MAX_REQUEST_BYTES = 8ll << 20;
    static constexpr TDuration DEFAULT_READ_DEADLINE = TDuration::Seconds(30);
    static constexpr ui64 DEFAULT_MAX_CACHE_DATA_SIZE = DefaultBlobCacheMaxBytes;
    static constexpr i64 DEFAULT_WRITE_PROTECT_DURATION_MS = 10800000;

    struct TCacheEntry {
        TString Data;

        TCacheEntry() = default;
        explicit TCacheEntry(TString data)
            : Data(std::move(data))
        {
        }
    };

    TLRUCache<TBlobRange, TCacheEntry> RegularCache;
    THashMap<TBlobRange, TCacheEntry> StickyCache;
    TSet<std::pair<TInstant, TBlobRange>> StickyOrder;
    /// List of cached ranges by blob id.
    /// It is used to remove all blob ranges from cache when
    /// it gets a notification that a blob has been deleted
    /// and to serve covering subrange reads of a cached blob.
    THashMultiSet<TBlobRange, BlobRangeHash, BlobRangeEqual> CachedRanges;

    TControlWrapper MaxCacheDataSize;
    const bool UseMaxCacheDataSizeFromConfig;
    TControlWrapper MaxInFlightDataSize;
    TControlWrapper WriteProtectDurationMs;
    const ui32 ShardIndex;
    const ui32 ShardCount;
    const TIntrusivePtr<TBlobCacheSharedState> SharedState;
    i64 CacheDataSize;   // Current size of all blobs in cache
    ui64 StickyBytes = 0;
    ui64 ReadCookie;
    THashMap<ui64, std::vector<TBlobRange>> CookieToRange;   // All in-flight requests
    THashMap<TBlobRange, TReadInfo> OutstandingReads;   // All in-flight and enqueued reads
    TDeque<TReadItem> ReadQueue;   // Reads that are waiting to be sent
        // TODO: Consider making per-group queues
    i64 InFlightDataSize;   // Current size of all in-flight blobs

    THashMap<ui64, TActorId> ShardPipes;   // TabletId -> PipeClient for small blob read requests
    THashMap<ui64, THashSet<ui64>> InFlightTabletRequests;   // TabletId -> list to read cookies

    using TCounterPtr = ::NMonitoring::TDynamicCounters::TCounterPtr;
    const TCounterPtr SizeBytes;
    const TCounterPtr SizeBlobs;
    const TCounterPtr Hits;
    const TCounterPtr Misses;
    const TCounterPtr Evictions;
    const TCounterPtr Adds;
    const TCounterPtr Forgets;
    const TCounterPtr HitsBytes;
    const TCounterPtr EvictedBytes;
    const TCounterPtr ReadBytes;
    const TCounterPtr ReadRangeFailedBytes;
    const TCounterPtr ReadRangeFailedCount;
    const TCounterPtr ReadSimpleFailedBytes;
    const TCounterPtr ReadSimpleFailedCount;
    const TCounterPtr AddBytes;
    const TCounterPtr ForgetBytes;
    const TCounterPtr SizeBytesInFlight;
    const TCounterPtr SizeBlobsInFlight;
    const TCounterPtr ReadRequests;
    const TCounterPtr ReadsInQueue;
    const TCounterPtr MaxSizeBytes;
    const TCounterPtr StickyBytesCounter;
    const TCounterPtr StickyBlobs;
    const TCounterPtr StickyHits;
    const TCounterPtr StickyHitsBytes;
    const TCounterPtr StickyEvictions;
    const TCounterPtr StickyEvictedBytes;

    TIntrusivePtr<NMemory::IMemoryConsumer> MemoryConsumer;

public:
    static constexpr auto ActorActivityType() {
        return NKikimrServices::TActivity::BLOB_CACHE_ACTOR;
    }

public:
    explicit TBlobCache(const std::optional<ui64>& maxSize, TIntrusivePtr<::NMonitoring::TDynamicCounters> counters, const ui32 shardIndex,
        const ui32 shardCount, TIntrusivePtr<TBlobCacheSharedState> sharedState)
        : TActorBootstrapped<TBlobCache>()
        , RegularCache(SIZE_MAX)
        , MaxCacheDataSize(maxSize.value_or(DEFAULT_MAX_CACHE_DATA_SIZE), 0, 1ull << 40)
        , UseMaxCacheDataSizeFromConfig(maxSize.has_value())
        , MaxInFlightDataSize(Min<i64>(MaxCacheDataSize, MAX_IN_FLIGHT_BYTES), 0, 10ull << 30)
        , WriteProtectDurationMs(DEFAULT_WRITE_PROTECT_DURATION_MS, 0, 86400000)
        , ShardIndex(shardIndex)
        , ShardCount(shardCount)
        , SharedState(std::move(sharedState))
        , CacheDataSize(0)
        , ReadCookie(1)
        , InFlightDataSize(0)
        , SizeBytes(counters->GetCounter("SizeBytes"))
        , SizeBlobs(counters->GetCounter("SizeBlobs"))
        , Hits(counters->GetCounter("Hits", true))
        , Misses(counters->GetCounter("Misses", true))
        , Evictions(counters->GetCounter("Evictions", true))
        , Adds(counters->GetCounter("Adds", true))
        , Forgets(counters->GetCounter("Forgets", true))
        , HitsBytes(counters->GetCounter("HitsBytes", true))
        , EvictedBytes(counters->GetCounter("EvictedBytes", true))
        , ReadBytes(counters->GetCounter("ReadBytes", true))
        , ReadRangeFailedBytes(counters->GetCounter("ReadRangeFailedBytes", true))
        , ReadRangeFailedCount(counters->GetCounter("ReadRangeFailedCount", true))
        , ReadSimpleFailedBytes(counters->GetCounter("ReadSimpleFailedBytes", true))
        , ReadSimpleFailedCount(counters->GetCounter("ReadSimpleFailedCount", true))
        , AddBytes(counters->GetCounter("AddBytes", true))
        , ForgetBytes(counters->GetCounter("ForgetBytes", true))
        , SizeBytesInFlight(counters->GetCounter("SizeBytesInFlight"))
        , SizeBlobsInFlight(counters->GetCounter("SizeBlobsInFlight"))
        , ReadRequests(counters->GetCounter("ReadRequests", true))
        , ReadsInQueue(counters->GetCounter("ReadsInQueue"))
        , MaxSizeBytes(counters->GetCounter("MaxSizeBytes"))
        , StickyBytesCounter(counters->GetCounter("StickyBytes"))
        , StickyBlobs(counters->GetCounter("StickyBlobs"))
        , StickyHits(counters->GetCounter("StickyHits", true))
        , StickyHitsBytes(counters->GetCounter("StickyHitsBytes", true))
        , StickyEvictions(counters->GetCounter("StickyEvictions", true))
        , StickyEvictedBytes(counters->GetCounter("StickyEvictedBytes", true))
    {
    }

    void Bootstrap(const TActorContext& ctx) {
        auto& icb = AppData(ctx)->Icb;
        TControlBoard::RegisterSharedControl(MaxCacheDataSize, icb->BlobCache.MaxCacheDataSize);
        TControlBoard::RegisterSharedControl(MaxInFlightDataSize, icb->BlobCache.MaxInFlightDataSize);
        TControlBoard::RegisterSharedControl(WriteProtectDurationMs, icb->BlobCache.WriteProtectDurationMs);

        LOG_S_NOTICE("BlobCache shard " << ShardIndex << "/" << ShardCount << " MaxCacheDataSize: " << GetLocalMaxCacheDataSize()
                                        << " MaxInFlightDataSize: " << GetLocalMaxInFlightDataSize());

        MaxSizeBytes->Set(GetLocalMaxCacheDataSize());

        if (ShardIndex == 0) {
            Send(NMemory::MakeMemoryControllerId(), new NMemory::TEvConsumerRegister(NMemory::EMemoryConsumerKind::ColumnTablesBlobCache));
        }

        Become(&TBlobCache::StateFunc);
        ScheduleWakeup();
    }

private:
    i64 GetLocalMaxCacheDataSize() const {
        i64 total = (i64)MaxCacheDataSize;
        if (SharedState && !UseMaxCacheDataSizeFromConfig) {
            total = SharedState->TotalMaxCacheDataSize.load(std::memory_order_relaxed);
        }
        if (ShardCount <= 1) {
            return total;
        }
        return std::max<i64>(total / static_cast<i64>(ShardCount), 1);
    }

    i64 GetLocalMaxInFlightDataSize() const {
        const i64 total = (i64)MaxInFlightDataSize;
        if (ShardCount <= 1) {
            return total;
        }
        return std::max<i64>(total / static_cast<i64>(ShardCount), 1);
    }

    STFUNC(StateFunc) {
        switch (ev->GetTypeRewrite()) {
            HFunc(TEvents::TEvPoisonPill, Handle);
            HFunc(TEvents::TEvWakeup, Handle);
            HFunc(TEvBlobCache::TEvReadBlobRange, Handle);
            HFunc(TEvBlobCache::TEvReadBlobRangeBatch, Handle);
            HFunc(TEvBlobCache::TEvCacheBlobRange, Handle);
            HFunc(TEvBlobCache::TEvForgetBlob, Handle);
            HFunc(TEvBlobStorage::TEvGetResult, Handle);
            HFunc(TEvTabletPipe::TEvClientConnected, Handle);
            HFunc(TEvTabletPipe::TEvClientDestroyed, Handle);
            HFunc(NMemory::TEvConsumerRegistered, Handle);
            HFunc(NMemory::TEvConsumerLimit, Handle);
            default:
                LOG_S_WARN("Unhandled event type: " << ev->GetTypeRewrite() << " event: " << ev->ToString());
                Send(IEventHandle::ForwardOnNondelivery(std::move(ev), TEvents::TEvUndelivered::ReasonActorUnknown));
                break;
        };
    }

    void ScheduleWakeup() {
        Schedule(TDuration::MilliSeconds(100), new TEvents::TEvWakeup());
    }

    void Handle(TEvents::TEvWakeup::TPtr& ev, const TActorContext& ctx) {
        Y_UNUSED(ev);
        ExpireSticky();
        Evict(ctx);   // Max cache size might have changed
        ScheduleWakeup();
    }

    void Handle(TEvents::TEvPoisonPill::TPtr& ev, const TActorContext& ctx) {
        Y_UNUSED(ev);
        Die(ctx);
    }

    void Handle(TEvBlobCache::TEvReadBlobRange::TPtr& ev, const TActorContext& ctx) {
        const TBlobRange& blobRange = ev->Get()->BlobRange;
        const bool promote = GetLocalMaxCacheDataSize() && ev->Get()->ReadOptions.CacheAfterRead;

        LOG_S_DEBUG("Read request: " << blobRange << " cache: " << (ui32)promote << " sender:" << ev->Sender);

        if (!HandleSingleRangeRead(TReadItem(ev->Get()->ReadOptions, blobRange), ev->Sender, ctx)) {
            MakeReadRequests(ctx);
        }
    }

    bool HandleSingleRangeRead(TReadItem readItem, const TActorId& sender, const TActorContext& ctx) {
        const TBlobRange& blobRange = readItem.BlobRange;
        YDB_LOG_DEBUG("",
            {"ask", blobRange});

        // Is in cache?
        if (auto cached = LookupCached(blobRange, readItem.PromoteInCache())) {
            Y_ABORT_UNLESS(cached->Data.size() == blobRange.Size, "Cached %s, size %" PRISZT, blobRange.ToString().c_str(), cached->Data.size());
            Hits->Inc();
            HitsBytes->Add(blobRange.Size);
            if (cached->Sticky) {
                StickyHits->Inc();
                StickyHitsBytes->Add(blobRange.Size);
            }
            SendResult(sender, blobRange, NKikimrProto::OK, cached->Data, {}, ctx, true);
            return true;
        }

        LOG_S_DEBUG("Miss cache: " << blobRange << " sender:" << sender);
        Misses->Inc();

        // Update set of outstanding requests.
        TReadInfo& blobInfo = OutstandingReads[blobRange];
        const bool inserted = blobInfo.Waiting.empty();

        blobInfo.Waiting.push_back(sender);
        blobInfo.Cache |= readItem.PromoteInCache();

        if (inserted) {
            LOG_S_DEBUG("Enqueue read range: " << blobRange);

            ReadQueue.emplace_back(std::move(readItem));
            ReadsInQueue->Set(ReadQueue.size());
            // The requested range just put into a read queue.
            // Extra work should be done to process the queue.
            return false;
        } else {
            // The requested range was already scheduled for read.
            return true;
        }
    }

    void Handle(TEvBlobCache::TEvReadBlobRangeBatch::TPtr& ev, const TActorContext& ctx) {
        const auto& ranges = ev->Get()->BlobRanges;
        LOG_S_DEBUG("Batch read request: " << JoinStrings(ranges.begin(), ranges.end(), " "));

        auto& readOptions = ev->Get()->ReadOptions;
        readOptions.CacheAfterRead = GetLocalMaxCacheDataSize() && readOptions.CacheAfterRead;

        for (const auto& blobRange : ranges) {
            HandleSingleRangeRead(TReadItem(readOptions, blobRange), ev->Sender, ctx);
        }

        MakeReadRequests(ctx);
    }

    void Handle(TEvBlobCache::TEvCacheBlobRange::TPtr& ev, const TActorContext& ctx) {
        const auto& blobRange = ev->Get()->BlobRange;
        const auto& data = ev->Get()->Data;

        if (blobRange.Size != data.size()) {
            LOG_S_ERROR("Trying to add invalid data for range: " << blobRange << " size: " << data.size());
            return;
        }

        Adds->Inc();

        if (OutstandingReads.contains(blobRange)) {
            // Don't bother if there is already a read request for this range
            return;
        }

        LOG_S_DEBUG("Adding range: " << blobRange);

        AddBytes->Add(blobRange.Size);

        InsertIntoCache(blobRange, data, ev->Get()->Sticky);

        Evict(ctx);
    }

    void Handle(TEvBlobCache::TEvForgetBlob::TPtr& ev, const TActorContext&) {
        const TUnifiedBlobId& blobId = ev->Get()->BlobId;

        LOG_S_INFO("Forgetting blob: " << blobId);

        Forgets->Inc();

        const auto [begin, end] = CachedRanges.equal_range(blobId);
        if (begin == end) {
            return;
        }

        TVector<TBlobRange> ranges(begin, end);
        for (const auto& range : ranges) {
            RemoveCachedRange(range);
        }
    }

    void Handle(NMemory::TEvConsumerRegistered::TPtr& ev, const TActorContext&) {
        MemoryConsumer = std::move(ev->Get()->Consumer);
    }

    void Handle(NMemory::TEvConsumerLimit::TPtr& ev, const TActorContext&) {
        if (UseMaxCacheDataSizeFromConfig) {
            return;
        }

        const i64 newMaxCacheDataSize = ev->Get()->LimitBytes;
        if (SharedState) {
            if (newMaxCacheDataSize == SharedState->TotalMaxCacheDataSize.load(std::memory_order_relaxed)) {
                return;
            }
            LOG_S_DEBUG("Updating max cache data size: " << newMaxCacheDataSize);
            SharedState->TotalMaxCacheDataSize.store(newMaxCacheDataSize, std::memory_order_relaxed);
        } else {
            if (newMaxCacheDataSize == (i64)MaxCacheDataSize) {
                return;
            }
            LOG_S_DEBUG("Updating max cache data size: " << newMaxCacheDataSize);
            MaxCacheDataSize = newMaxCacheDataSize;
        }

        MaxSizeBytes->Set(GetLocalMaxCacheDataSize());
    }

    void UpdateConsumption() {
        if (SharedState) {
            SharedState->SetShardCacheDataSize(ShardIndex, CacheDataSize);
        }
        if (!MemoryConsumer) {
            return;
        }

        MemoryConsumer->SetConsumption(SharedState ? SharedState->GetTotalCacheDataSize() : CacheDataSize);
    }

    void SendBatchReadRequestToDS(const std::vector<TBlobRange>& blobRanges, const ui64 cookie, ui32 dsGroup,
        TReadItem::EReadVariant readVariant, const TActorContext& ctx) {
        LOG_S_DEBUG("Sending read from BlobCache: group: " << dsGroup << " ranges: " << JoinStrings(blobRanges.begin(), blobRanges.end(), " ")
                                                           << " cookie: " << cookie);

        TArrayHolder<TEvBlobStorage::TEvGet::TQuery> queires(new TEvBlobStorage::TEvGet::TQuery[blobRanges.size()]);
        for (size_t i = 0; i < blobRanges.size(); ++i) {
            Y_ABORT_UNLESS(dsGroup == blobRanges[i].BlobId.GetDsGroup());
            queires[i].Set(blobRanges[i].BlobId.GetLogoBlobId(), blobRanges[i].Offset, blobRanges[i].Size);
        }

        NKikimrBlobStorage::EGetHandleClass readClass = TReadItem::ReadClass(readVariant);
        TInstant deadline = ReadDeadline(readVariant);
        SendToBSProxy(ctx, dsGroup, new TEvBlobStorage::TEvGet(queires, blobRanges.size(), deadline, readClass, false), cookie);

        ReadRequests->Inc();
    }

    static TInstant ReadDeadline(TReadItem::EReadVariant variant) {
        if (variant == TReadItem::EReadVariant::DEFAULT) {
            return TAppData::TimeProvider->Now() + DEFAULT_READ_DEADLINE;
        }
        // We want to wait for data anyway in this case. This behaviour is similar to datashard
        return TInstant::Max();   // EReadVariant::DEFAULT_NO_DEADLINE || EReadVariant::FAST
    }

    void MakeReadRequests(const TActorContext& ctx) {
        THashMap<std::tuple<ui64, ui32, TReadItem::EReadVariant>, std::vector<TBlobRange>> groupedBlobRanges;

        while (!ReadQueue.empty()) {
            const auto& readItem = ReadQueue.front();
            const TBlobRange& blobRange = readItem.BlobRange;

            // NOTE: if queue is not empty, at least 1 in-flight request is allowed
            if (InFlightDataSize && InFlightDataSize >= GetLocalMaxInFlightDataSize()) {
                break;
            }
            InFlightDataSize += blobRange.Size;
            SizeBytesInFlight->Add(blobRange.Size);
            SizeBlobsInFlight->Inc();

            auto blobSrc = readItem.BlobSource();
            groupedBlobRanges[blobSrc].push_back(blobRange);

            ReadQueue.pop_front();
        }

        ReadsInQueue->Set(ReadQueue.size());

        // We might need to free some space to accommodate the results of new reads
        Evict(ctx);

        ui64 cookie = ++ReadCookie;

        // TODO: fix small blobs mix with dsGroup == 0 (it could be zero in tests)
        for (auto& [target, rangesGroup] : groupedBlobRanges) {
            ui64 requestSize = 0;
            ui32 dsGroup = std::get<1>(target);
            TReadItem::EReadVariant readVariant = std::get<2>(target);

            std::vector<ui64> dsReads;

            for (auto& blobRange : rangesGroup) {
                if (requestSize && (requestSize + blobRange.Size > MAX_REQUEST_BYTES)) {
                    dsReads.push_back(cookie);
                    cookie = ++ReadCookie;
                    requestSize = 0;
                }

                requestSize += blobRange.Size;
                CookieToRange[cookie].emplace_back(std::move(blobRange));
            }
            if (requestSize) {
                dsReads.push_back(cookie);
                cookie = ++ReadCookie;
                requestSize = 0;
            }

            for (ui64 cookie : dsReads) {
                SendBatchReadRequestToDS(CookieToRange[cookie], cookie, dsGroup, readVariant, ctx);
            }
        }
    }

    void SendResult(const TActorId& to, const TBlobRange& blobRange, NKikimrProto::EReplyStatus status, const TString& data,
        const TString& detailedError, const TActorContext& ctx, const bool fromCache = false) {
        LOG_S_DEBUG("Send result: " << blobRange << " to: " << to << " status: " << status);

        ctx.Send(to, new TEvBlobCache::TEvReadBlobRangeResult(blobRange, status, data, detailedError, fromCache));
    }

    void Handle(TEvBlobStorage::TEvGetResult::TPtr& ev, const TActorContext& ctx) {
        const ui64 readCookie = ev->Cookie;

        if (ev->Get()->ResponseSz < 1) {
            Y_ABORT("Unexpected reply from blobstorage");
        }

        TString detailedError;
        if (ev->Get()->Status != NKikimrProto::EReplyStatus::OK) {
            detailedError = ev->Get()->ToString();
            YDB_LOG_WARN("",
                {"fail", ev->Get()->ToString()});
            ReadSimpleFailedBytes->Add(ev->Get()->ResponseSz);
            ReadSimpleFailedCount->Add(1);
        } else {
            YDB_LOG_DEBUG("",
                {"success", ev->Get()->ToString()});
        }

        auto cookieIt = CookieToRange.find(readCookie);
        if (cookieIt == CookieToRange.end()) {
            // This shouldn't happen
            LOG_S_CRIT("Unknown read result cookie: " << readCookie);
            return;
        }

        std::vector<TBlobRange> blobRanges = std::move(cookieIt->second);
        CookieToRange.erase(readCookie);

        Y_ABORT_UNLESS(blobRanges.size() == ev->Get()->ResponseSz, "Mismatched number of results for read request!");

        for (size_t i = 0; i < ev->Get()->ResponseSz; ++i) {
            const auto& res = ev->Get()->Responses[i];
            ProcessSingleRangeResult(blobRanges[i], readCookie, res.Status, res.Buffer.ConvertToString(), detailedError, ctx);
        }

        MakeReadRequests(ctx);
    }

    void ProcessSingleRangeResult(const TBlobRange& blobRange, const ui64 readCookie, ui32 status, const TString& data,
        const TString& detailedError, const TActorContext& ctx) noexcept {
        YDB_LOG_DEBUG("",
            {"processSingleRangeResult", blobRange});
        auto readIt = OutstandingReads.find(blobRange);
        if (readIt == OutstandingReads.end()) {
            // This shouldn't happen
            LOG_S_CRIT("Unknown read result key: " << blobRange << " cookie: " << readCookie);
            return;
        }

        SizeBytesInFlight->Sub(blobRange.Size);
        SizeBlobsInFlight->Dec();
        InFlightDataSize -= blobRange.Size;

        Y_ABORT_UNLESS(!HasCached(blobRange), "Range %s must not be already in cache", blobRange.ToString().c_str());

        if (status == NKikimrProto::EReplyStatus::OK) {
            Y_ABORT_UNLESS(blobRange.Size == data.size(), "Read %s, size %" PRISZT, blobRange.ToString().c_str(), data.size());
            ReadBytes->Add(blobRange.Size);

            if (readIt->second.Cache) {
                InsertIntoCache(blobRange, data, false);
            }
        } else {
            LOG_S_WARN("Read failed for range: " << blobRange << " status: " << NKikimrProto::EReplyStatus_Name(status));
            ReadRangeFailedBytes->Add(blobRange.Size);
            ReadRangeFailedCount->Add(1);
        }

        YDB_LOG_DEBUG("",
            {"processSingleRangeResult", blobRange},
            {"sendReplies", readIt->second.Waiting.size()});
        // Send results to all waiters
        for (const auto& to : readIt->second.Waiting) {
            SendResult(to, blobRange, (NKikimrProto::EReplyStatus)status, data, detailedError, ctx);
        }

        OutstandingReads.erase(readIt);
    }

    // Forgets the pipe to the tablet and fails all in-flight requests to it
    void DestroyPipe(ui64 tabletId, const TActorContext& ctx) {
        ShardPipes.erase(tabletId);
        // Send errors for in-flight requests
        auto cookies = std::move(InFlightTabletRequests[tabletId]);
        InFlightTabletRequests.erase(tabletId);
        for (ui64 readCookie : cookies) {
            auto cookieIt = CookieToRange.find(readCookie);
            if (cookieIt == CookieToRange.end()) {
                // This might only happen in case fo race between response and pipe close
                LOG_S_NOTICE("Unknown read result cookie: " << readCookie);
                return;
            }

            std::vector<TBlobRange> blobRanges = std::move(cookieIt->second);
            CookieToRange.erase(readCookie);

            for (size_t i = 0; i < blobRanges.size(); ++i) {
                Y_ABORT_UNLESS(blobRanges[i].BlobId.GetTabletId() == tabletId);
                ProcessSingleRangeResult(blobRanges[i], readCookie, NKikimrProto::EReplyStatus::NOTREADY, {}, {}, ctx);
            }
        }

        MakeReadRequests(ctx);
    }

    void Handle(TEvTabletPipe::TEvClientConnected::TPtr& ev, const TActorContext& ctx) {
        TEvTabletPipe::TEvClientConnected* msg = ev->Get();
        const ui64 tabletId = msg->TabletId;
        Y_ABORT_UNLESS(tabletId != 0);
        if (msg->Status == NKikimrProto::OK) {
            LOG_S_DEBUG("Pipe connected to tablet: " << tabletId);
        } else {
            LOG_S_DEBUG("Pipe connection to tablet: " << tabletId << " failed with status: " << msg->Status);
            DestroyPipe(tabletId, ctx);
        }
    }

    void Handle(TEvTabletPipe::TEvClientDestroyed::TPtr& ev, const TActorContext& ctx) {
        const ui64 tabletId = ev->Get()->TabletId;
        Y_ABORT_UNLESS(tabletId != 0);

        LOG_S_DEBUG("Closed pipe connection to tablet: " << tabletId);
        DestroyPipe(tabletId, ctx);
    }

    void InsertIntoCache(const TBlobRange& blobRange, TString data, const bool sticky) {
        // Shrink the buffer if it has to much extra capacity
        if (data.capacity() > data.size() * 1.1) {
            data = TString(data.begin(), data.end());
        }
        YDB_LOG_DEBUG("",
            {"insertCache", blobRange},
            {"sticky", sticky});
        if (HasCached(blobRange) || !GetLocalMaxCacheDataSize()) {
            return;
        }

        TCacheEntry entry(std::move(data));
        const TInstant now = TAppData::TimeProvider->Now();
        const TDuration protectFor = TDuration::MilliSeconds((i64)WriteProtectDurationMs);
        const bool makeSticky = sticky && protectFor && now + protectFor > now;

        if (makeSticky) {
            const TInstant stickyUntil = now + protectFor;
            AFL_VERIFY(StickyCache.emplace(blobRange, entry).second);
            StickyOrder.emplace(stickyUntil, blobRange);
            StickyBytes += blobRange.Size;
            StickyBytesCounter->Set(StickyBytes);
            StickyBlobs->Set(StickyCache.size());
        } else if (!RegularCache.Insert(blobRange, entry)) {
            return;
        }

        CachedRanges.insert(blobRange);
        CacheDataSize += blobRange.Size;
        SizeBytes->Add(blobRange.Size);
        SizeBlobs->Inc();

        UpdateConsumption();
    }

    static bool Covers(const TBlobRange& cached, const TBlobRange& requested) {
        return cached.Offset <= requested.Offset &&
               static_cast<ui64>(cached.Offset) + cached.Size >= static_cast<ui64>(requested.Offset) + requested.Size;
    }

    bool HasCached(const TBlobRange& blobRange) const {
        return StickyCache.contains(blobRange) || RegularCache.FindWithoutPromote(blobRange) != RegularCache.End();
    }

    struct TCachedLookup {
        TString Data;
        bool Sticky = false;
    };

    std::optional<TCachedLookup> LookupCached(const TBlobRange& blobRange, const bool promote) {
        if (auto exact = LookupExact(blobRange, promote)) {
            return exact;
        }
        return LookupCovering(blobRange, promote);
    }

    std::optional<TCachedLookup> LookupExact(const TBlobRange& blobRange, const bool promote) {
        if (auto it = StickyCache.find(blobRange); it != StickyCache.end()) {
            return TCachedLookup{it->second.Data, true};
        }
        if (promote) {
            auto it = RegularCache.Find(blobRange);
            if (it != RegularCache.End()) {
                return TCachedLookup{it.Value().Data, false};
            }
        } else {
            auto it = RegularCache.FindWithoutPromote(blobRange);
            if (it != RegularCache.End()) {
                return TCachedLookup{it.Value().Data, false};
            }
        }
        return std::nullopt;
    }

    std::optional<TCachedLookup> LookupCovering(const TBlobRange& blobRange, const bool promote) {
        const auto [begin, end] = CachedRanges.equal_range(blobRange.BlobId);
        for (auto it = begin; it != end; ++it) {
            if (*it == blobRange || !Covers(*it, blobRange)) {
                continue;
            }
            auto covering = LookupExact(*it, promote);
            if (!covering) {
                continue;
            }
            const ui32 shift = blobRange.Offset - it->Offset;
            Y_ABORT_UNLESS(shift + blobRange.Size <= covering->Data.size());
            return TCachedLookup{covering->Data.substr(shift, blobRange.Size), covering->Sticky};
        }
        return std::nullopt;
    }

    void UnregisterCachedRangeIndex(const TBlobRange& blobRange) {
        const auto [begin, end] = CachedRanges.equal_range(blobRange.BlobId);
        for (auto it = begin; it != end; ++it) {
            if (*it == blobRange) {
                CachedRanges.erase(it);
                return;
            }
        }
    }

    void RemoveCachedRange(const TBlobRange& blobRange) {
        if (auto stickyIt = StickyCache.find(blobRange); stickyIt != StickyCache.end()) {
            for (auto orderIt = StickyOrder.begin(); orderIt != StickyOrder.end(); ++orderIt) {
                if (orderIt->second == blobRange) {
                    StickyOrder.erase(orderIt);
                    break;
                }
            }
            StickyCache.erase(stickyIt);
            StickyBytes -= blobRange.Size;
            StickyBytesCounter->Set(StickyBytes);
            StickyBlobs->Set(StickyCache.size());
        } else {
            auto it = RegularCache.FindWithoutPromote(blobRange);
            if (it == RegularCache.End()) {
                return;
            }
            RegularCache.Erase(it);
        }

        UnregisterCachedRangeIndex(blobRange);
        CacheDataSize -= blobRange.Size;
        SizeBytes->Set(CacheDataSize);
        SizeBlobs->Set(RegularCache.Size() + StickyCache.size());
        ForgetBytes->Add(blobRange.Size);
        UpdateConsumption();
    }

    void ExpireSticky() {
        const TInstant now = TAppData::TimeProvider->Now();
        while (!StickyOrder.empty() && StickyOrder.begin()->first <= now) {
            const TBlobRange range = StickyOrder.begin()->second;
            StickyOrder.erase(StickyOrder.begin());
            auto stickyIt = StickyCache.find(range);
            if (stickyIt == StickyCache.end()) {
                continue;
            }
            TCacheEntry entry = std::move(stickyIt->second);
            StickyCache.erase(stickyIt);
            StickyBytes -= range.Size;
            StickyBytesCounter->Set(StickyBytes);
            StickyBlobs->Set(StickyCache.size());
            if (!RegularCache.Insert(range, entry)) {
                UnregisterCachedRangeIndex(range);
                CacheDataSize -= range.Size;
                SizeBytes->Set(CacheDataSize);
                SizeBlobs->Set(RegularCache.Size() + StickyCache.size());
            }
        }
        UpdateConsumption();
    }

    void Evict(const TActorContext&) {
        ExpireSticky();
        while (CacheDataSize + InFlightDataSize > GetLocalMaxCacheDataSize()) {
            TBlobRange victim;
            bool stickyVictim = false;
            if (RegularCache.Size()) {
                auto it = RegularCache.FindOldest();
                if (it == RegularCache.End()) {
                    break;
                }
                victim = it.Key();
            } else if (!StickyOrder.empty()) {
                victim = StickyOrder.begin()->second;
                stickyVictim = true;
            } else {
                break;
            }

            LOG_S_DEBUG("Evict: " << victim << " sticky: " << stickyVictim << " CacheDataSize: " << CacheDataSize
                                  << " InFlightDataSize: " << (i64)InFlightDataSize
                                  << " MaxCacheDataSize: " << GetLocalMaxCacheDataSize());

            Evictions->Inc();
            EvictedBytes->Add(victim.Size);
            if (stickyVictim) {
                // Sticky victim is still inside WriteProtectDurationMs (default 3h).
                StickyEvictions->Inc();
                StickyEvictedBytes->Add(victim.Size);
            }

            // RemoveCachedRange already updates SizeBytes/SizeBlobs/CacheDataSize
            // but also increments ForgetBytes which is wrong for eviction.
            // Split: do eviction accounting here.
            if (stickyVictim) {
                auto stickyIt = StickyCache.find(victim);
                AFL_VERIFY(stickyIt != StickyCache.end());
                StickyOrder.erase(StickyOrder.begin());
                StickyCache.erase(stickyIt);
                StickyBytes -= victim.Size;
                StickyBytesCounter->Set(StickyBytes);
                StickyBlobs->Set(StickyCache.size());
            } else {
                auto it = RegularCache.FindWithoutPromote(victim);
                AFL_VERIFY(it != RegularCache.End());
                RegularCache.Erase(it);
            }
            UnregisterCachedRangeIndex(victim);
            CacheDataSize -= victim.Size;
            SizeBytes->Set(CacheDataSize);
            SizeBlobs->Set(RegularCache.Size() + StickyCache.size());
        }

        UpdateConsumption();
    }
};

}   // namespace

void SendReadBlobRangeBatch(std::vector<TBlobRange>&& blobRanges, TReadBlobRangeOptions opts) {
    if (blobRanges.empty()) {
        return;
    }
    std::array<std::vector<TBlobRange>, BlobCacheShardCount> rangesByShard;
    for (auto&& range : blobRanges) {
        rangesByShard[BlobCacheShardIndex(range.BlobId)].emplace_back(std::move(range));
    }
    auto& ctx = NActors::TActorContext::AsActorContext();
    for (ui32 shard = 0; shard < BlobCacheShardCount; ++shard) {
        if (rangesByShard[shard].empty()) {
            continue;
        }
        ctx.Send(MakeBlobCacheServiceId(shard),
            new TEvBlobCache::TEvReadBlobRangeBatch(std::move(rangesByShard[shard]), TReadBlobRangeOptions(opts)));
    }
}

NActors::IActor* CreateBlobCache(const std::optional<ui64>& maxBytes, TIntrusivePtr<::NMonitoring::TDynamicCounters> counters) {
    return CreateBlobCache(maxBytes, counters, 0, 1, {});
}

NActors::IActor* CreateBlobCache(const std::optional<ui64>& maxBytes, TIntrusivePtr<::NMonitoring::TDynamicCounters> counters,
    const ui32 shardIndex, const ui32 shardCount, TIntrusivePtr<TBlobCacheSharedState> sharedState) {
    AFL_VERIFY(shardCount >= 1);
    AFL_VERIFY(shardIndex < shardCount);
    if (shardCount > 1) {
        AFL_VERIFY(sharedState);
        AFL_VERIFY(sharedState->ShardCount == shardCount);
    }
    if (shardCount > 1) {
        counters = counters->GetSubgroup("shard", ToString(shardIndex));
    }
    return new TBlobCache(maxBytes, counters, shardIndex, shardCount, std::move(sharedState));
}

}   // namespace NKikimr::NBlobCache
