#pragma once

#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    enum {
        EvDone = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
        EvWriteValueRequest,
        EvLast
    };

    struct TEvDone : NActors::TEventLocal<TEvDone, EvDone> {
        int64_t Value;
        TEvDone(int64_t value)
            : Value(value)
        {}
    };

    struct TEvWriteValueRequest : NActors::TEventLocal<TEvWriteValueRequest, EvWriteValueRequest> {
        int64_t Value;
        TEvWriteValueRequest(int64_t value)
            : Value(value)
        {}
    };
};
