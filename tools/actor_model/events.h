#pragma once

#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {

    enum Events {
        ForTEvDone = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
        ForRequest
    };

    struct TEvWriteValueRequest : public NActors::TEventLocal<TEvWriteValueRequest, ForRequest> {
        int64_t value;

        explicit TEvWriteValueRequest(int64_t val) 
            : value(val) {
        }
    };

    struct TEvDone : public NActors::TEventLocal<TEvDone, ForTEvDone> {
    };

};
