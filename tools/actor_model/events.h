#pragma once

#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>
#include <library/cpp/actors/core/actorid.h>

struct TEvents {
    enum EEv : ui32 {
        EvBegin = NActors::TEvents::ES_PRIVATE,

        EvWriteValueRequest = EvBegin + 1,
        EvDone,

        EvEnd
    };

    static_assert(EvEnd < NActors::TEvents::ES_PRIVATE + 1024, "Too many private events");

    struct TEvWriteValueRequest : NActors::TEventLocal<TEvWriteValueRequest, EvWriteValueRequest> {
        int64_t Value;

        TEvWriteValueRequest(int64_t value) : Value(value) {}
    };
    struct TEvDone : NActors::TEventLocal<TEvDone, EvDone> {};
};
