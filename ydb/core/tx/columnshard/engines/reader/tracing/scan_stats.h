#pragma once

#include <util/generic/hash.h>
#include <util/generic/string.h>
#include <util/string/builder.h>

namespace NKikimr::NOlap::NReader {

inline TString IndexClassTraceName(const TString& className) {
    if (className == "MIN_MAX") {
        return "min_max";
    } else if (className == "MAX") {
        return "max";
    } else if (className == "BLOOM_FILTER") {
        return "bloom";
    } else if (className == "BLOOM_NGRAMM_FILTER") {
        return "bloom_ngram";
    } else if (className == "CATEGORY_BLOOM_FILTER") {
        return "category_bloom";
    }
    return className ? className : TString("none");
}

struct TIndexCheckStats {
    ui32 NoIndex = 0;
    ui32 AllAccepted = 0;
    ui32 AllDenied = 0;
    ui32 Partial = 0;

    void Add(const TString& status) {
        if (status == "NoIndex") {
            ++NoIndex;
        } else if (status == "AllAccepted") {
            ++AllAccepted;
        } else if (status == "AllDenied") {
            ++AllDenied;
        } else if (status == "Partial") {
            ++Partial;
        }
    }

    void Merge(const TIndexCheckStats& other) {
        NoIndex += other.NoIndex;
        AllAccepted += other.AllAccepted;
        AllDenied += other.AllDenied;
        Partial += other.Partial;
    }

    bool Empty() const {
        return !NoIndex && !AllAccepted && !AllDenied && !Partial;
    }

    TString ToJson() const {
        TStringBuilder sb;
        sb << "{\"NoIndex\":" << NoIndex << ",\"AllAccepted\":" << AllAccepted << ",\"AllDenied\":" << AllDenied
           << ",\"Partial\":" << Partial << "}";
        return sb;
    }
};

struct TScanReadTraceStats {
    ui64 CacheBytes = 0;
    ui64 BsBytes = 0;
    ui64 TierBytes = 0;
    THashMap<TString, TIndexCheckStats> Indexes;

    void AddIndexCheck(const TString& className, const TString& status) {
        Indexes[IndexClassTraceName(className)].Add(status);
    }

    TString IndexesToJson() const {
        TStringBuilder sb;
        sb << "{";
        bool first = true;
        for (const auto& [name, stats] : Indexes) {
            if (stats.Empty()) {
                continue;
            }
            if (!first) {
                sb << ",";
            }
            first = false;
            sb << "\"" << name << "\":" << stats.ToJson();
        }
        sb << "}";
        return sb;
    }

    TString ToDetailsJson() const {
        TStringBuilder sb;
        sb << "{\"indexes\":" << IndexesToJson() << "}";
        return sb;
    }
};

}   // namespace NKikimr::NOlap::NReader
