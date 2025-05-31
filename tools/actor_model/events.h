#pragma once

#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    enum EEventType {
        EvWriteValueRequest = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
        EvDone,
        EvPoisonPill
    };

    struct TEvDone : NActors::TEventLocal<TEvDone, EvDone> {};
    struct TEvWriteValueRequest : NActors::TEventLocal<TEvWriteValueRequest, EvWriteValueRequest> {
        int64_t Value;
        explicit TEvWriteValueRequest(int64_t value) : Value(value) {}
    };
    struct TEvPoisonPill : NActors::TEventLocal<TEvPoisonPill, EvPoisonPill> {};
};
