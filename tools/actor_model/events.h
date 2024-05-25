#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents{
    enum EEv {
        EventResponse = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
        EventFinish
    };

    struct TEvDone : NActors::TEventLocal<TEvDone, EventFinish> {};

    struct TEvWriteValueRequest : NActors::TEventLocal<TEvWriteValueRequest, EventResponse> {
        int64_t value;
        explicit TEvWriteValueRequest(int64_t value) : value(value) {}
    };
};