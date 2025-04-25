#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    enum Events {
        EventResponse = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
        EventDone
    };

    struct TEvDone : NActors::TEventLocal<TEvDone, EventDone> {};

    struct TEvWriteValueRequest : NActors::TEventLocal<TEvWriteValueRequest, EventResponse> {
        int64_t value;
        explicit TEvWriteValueRequest(int Value) : value(Value) {}
    };
};
