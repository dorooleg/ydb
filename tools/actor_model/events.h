#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    // Вам нужно самостоятельно сюда добавить все необходимые events в NActors::TEvents::ES_PRIVATE
     enum {
        EvWriteValueRequest = NActors::TEvents::ES_PRIVATE,
        EvDone
    };

    struct TEvWriteValueRequest : public NActors::TEventLocal<TEvWriteValueRequest, EvWriteValueRequest> {
        int64_t Value;
        TEvWriteValueRequest(int64_t value) : Value(value) {}
    };

    struct TEvDone : public NActors::TEventLocal<TEvDone, EvDone> {};
};
