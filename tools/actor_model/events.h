#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    // Вам нужно самостоятельно сюда добавить все необходимые events в NActors::TEvents::ES_PRIVATE
    enum events {
        EvValue = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
        Done
    };

    struct TEvWriteValueRequest : NActors::TEventLocal<TEvWriteValueRequest, EvValue> {
        int64_t Value;
        explicit TEvWriteValueRequest(int64_t value) : Value(value) {}
    };
    struct TEvDone : NActors::TEventLocal<TEvDone, Done> {};
};
