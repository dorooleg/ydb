#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    // Вам нужно самостоятельно сюда добавить все необходимые events в NActors::TEvents::ES_PRIVATE

    enum {
        EvWriteValueRequest = EventSpaceBegin(NActors::TEvents::ES_PRIVATE), // начать с диапазона блока ES_PRIVATE
        EvDone
    };

    struct TEvDone : NActors::TEventLocal<TEvDone, EvDone> {};

    struct TEvWriteValueRequest : NActors::TEventLocal<TEvWriteValueRequest, EvWriteValueRequest> {
        int64_t Num;
        explicit TEvWriteValueRequest(int64_t value) : Num(value) {} // Явное приведение
    };
};