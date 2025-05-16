#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    enum EventIDs {
        EvWriteValueRequest = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
        EvDone
    };
    struct TEvWriteValueRequest : NActors::TEventLocal<TEvWriteValueRequest, EvWriteValueRequest> {
        int64_t Value;
        TEvWriteValueRequest(int64_t value) : Value(value) {}
    };
    struct TEvDone : NActors::TEventLocal<TEvDone, EvDone> {};
};
