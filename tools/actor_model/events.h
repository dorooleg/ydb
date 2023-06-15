#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents
{
    // Вам нужно самостоятельно сюда добавить все необходимые events в NActors::TEvents::ES_PRIVATE

    enum EEv
    {
        EvDone,
        EvRequest = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
    };

    struct TEvDone : NActors::TEventLocal<TEvDone, EvDone>
    {
    };

    struct TEvWriteValueRequest : NActors::TEventLocal<TEvWriteValueRequest, EvRequest>
    {
        int64_t Value;
        explicit TEvWriteValueRequest(int64_t value) : Value(value) {}
    };
};
