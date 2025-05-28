#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    struct TEvDone : public NActors::TEventLocal<TEvDone, NActors::TEvents::ES_PRIVATE> {};
    
    struct TEvWriteValueRequest : public NActors::TEventLocal<TEvWriteValueRequest, NActors::TEvents::ES_PRIVATE> {
        int64_t Value;
        
        TEvWriteValueRequest(int64_t value)
            : Value(value)
        {}
    };
};
