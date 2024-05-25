#ifndef EVENTS_H
#define EVENTS_H

#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

namespace TEvents {
    enum {
        EvWriteValueRequest = NActors::TEvents::ES_PRIVATE,
        EvDone
    };

    struct TEvWriteValueRequest : public NActors::TEventLocal<TEvWriteValueRequest, EvWriteValueRequest> {
        int64_t Value;

        TEvWriteValueRequest(int64_t value) : Value(value) {}
    };

    struct TEvDone : public NActors::TEventLocal<TEvDone, EvDone> {};
}

#endif 
