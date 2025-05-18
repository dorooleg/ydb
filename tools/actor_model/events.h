#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    struct TEvWakeup: public NActors::TEventLocal<TEvWakeup, 1>{};

    struct TEvDone: public NActors::TEventLocal<TEvDone, 2>{};

    struct TEvPoisonPill: public NActors::TEventLocal<TEvPoisonPill, 3>{};

    struct TEvWriteValueRequest : public NActors::TEventLocal<TEvWriteValueRequest, 4> {
        int Value;
        TEvWriteValueRequest(int Value) {
            this->Value = Value;
        }
    };
    // Вам нужно самостоятельно сюда добавить все необходимые events в NActors::TEvents::ES_PRIVATE
};
