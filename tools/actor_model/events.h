#pragma once
#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    enum EEv {
        EvWriteValueRequest = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
        EvDone,
        EvEnd
    };

    static_assert(EvEnd < EventSpaceEnd(NActors::TEvents::ES_PRIVATE), "Event space overflow");

    struct TEvWriteValueRequest : NActors::TEventLocal<TEvWriteValueRequest, EvWriteValueRequest> {
        int64_t Value;

        TEvWriteValueRequest(int64_t val) : Value(val) {}
    };

    struct TEvDone : NActors::TEventLocal<TEvDone, EvDone> {};
};
