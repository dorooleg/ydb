#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    // Вам нужно самостоятельно сюда добавить все необходимые events в NActors::TEvents::ES_PRIVATE
    struct TEvWriteValueRequest : NActors::TEventLocal<TEvWriteValueRequest, NActors::TEvents::ES_PRIVATE + 1> {
        int64_t Value;
        TEvWriteValueRequest(int64_t value) : Value(value) {}
    };

    struct TEvDone : NActors::TEventLocal<TEvDone, NActors::TEvents::ES_PRIVATE + 2> {};
};
