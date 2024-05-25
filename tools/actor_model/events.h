#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    enum Events {
        EventRequest = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
        EventDone
    };

    struct TEvDone : NActors::TEventLocal<TEvDone, EventDone> {};

    struct TEvWriteValueRequest : NActors::TEventLocal<TEvWriteValueRequest, EventRequest> {
        int64_t Value;
        explicit TEvWriteValueRequest(int64_t value) : Value(value) {}
    };
};
