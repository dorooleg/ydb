#pragma once

#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    enum EEv {
        EvDone = NActors::TEvents::ES_PRIVATE + 1,
        EvWriteData = NActors::TEvents::ES_PRIVATE + 2
    };

    struct TEvDone : NActors::TEventLocal<TEvDone, EEv::EvDone> {
        TEvDone() = default;
    };

    struct TEvWriteData : NActors::TEventLocal<TEvWriteData, EEv::EvWriteData> {
        int64_t Value;
        TEvWriteData(int64_t value) : Value(value) {}
    };
};
