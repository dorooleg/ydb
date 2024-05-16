#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    // Создаем идентификаторы для событий в пространстве имен NActors::TEvents::ES_PRIVATE
    enum {
        EvWakeup = EventSpaceBegin(TEventSpace::ES_PRIVATE),
        EvDone,
        EvWriteValueRequest,
        EvEnd
    };

    struct TEvWakeup : public NActors::TEventLocal<TEvWakeup, EvWakeup> {
        TEvWakeup() = default;
    };

    struct TEvDone : public NActors::TEventLocal<TEvDone, EvDone> {
        TEvDone() = default;
    };

    struct TEvWriteValueRequest : public NActors::TEventLocal<TEvWriteValueRequest, EvWriteValueRequest> {
        explicit TEvWriteValueRequest(int64_t value)
                : Value(value) {}

        int64_t Value;
    };
};