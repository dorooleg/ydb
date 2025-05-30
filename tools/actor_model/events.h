#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents
{
    enum EEv
    {
        EvWriteValueRequest = NActors::TEvents::ES_PRIVATE + 1,
        EvDone = NActors::TEvents::ES_PRIVATE + 2,
        EvEnd = NActors::TEvents::ES_PRIVATE + 3
    };
    
    struct TEvWriteValueRequest : NActors::TEventLocal<TEvWriteValueRequest, EvWriteValueRequest>
    {
        int64_t Value;
        TEvWriteValueRequest(int64_t value) : Value(value) {}
    };

    struct TEvDone : NActors::TEventLocal<TEvDone, EvDone>
    {
    };
};