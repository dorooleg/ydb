#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
     enum Events {
        Response = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
        Done
    };

    struct TEvDone : NActors::TEventLocal<TEvDone, Done> {};

    struct TEvWriteValueRequest : NActors::TEventLocal<TEvWriteValueRequest, Response> {
        int64_t Value;
        explicit TEvWriteValueRequest(int64_t value) : Value(value) {}
    };
};