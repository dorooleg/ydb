#include "remove.h"

#include <ydb/core/tx/columnshard/blob_cache.h>

#include <ydb/library/actors/core/log.h>

namespace NKikimr::NOlap {

void IBlobsDeclareRemovingAction::DeclareRemove(const TTabletId tabletId, const TUnifiedBlobId& blobId) {
    if (DeclaredBlobs.Add(tabletId, blobId)) {
        YDB_LOG_DEBUG_COMP(NActors::NStructuredLog::TLogStack::GetComponent(), "",
            {"event", "DeclareRemove"},
            {"blobId", blobId},
            {"tabletId", (ui64)tabletId});
        Counters->OnRequest(blobId.BlobSize());
        NBlobCache::ForgetBlob(blobId);
        return DoDeclareRemove(tabletId, blobId);
    }
}

void IBlobsDeclareRemovingAction::DeclareSelfRemove(const TUnifiedBlobId& blobId) {
    DeclareRemove(SelfTabletId, blobId);
}

void IBlobsDeclareRemovingAction::OnCompleteTxAfterRemoving(const bool blobsWroteSuccessfully) {
    DoOnCompleteTxAfterRemoving(blobsWroteSuccessfully);
    if (blobsWroteSuccessfully) {
        for (auto i = DeclaredBlobs.GetIterator(); i.IsValid(); ++i) {
            NBlobCache::ForgetBlob(i.GetBlobId());
        }
    }
}

}   // namespace NKikimr::NOlap
