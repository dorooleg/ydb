#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    enum EEv {
        EvDone = NActors::TEvents::ES_PRIVATE,
        EvWriteValueRequest,
        EvEnd
    };

    struct TEvDone : NActors::TEventLocal<TEvDone, EvDone> {};
    
    struct TEvWriteValueRequest : NActors::TEventLocal<TEvWriteValueRequest, EvWriteValueRequest> {
        int64_t Value;
        explicit TEvWriteValueRequest(int64_t value) : Value(value) {}
    };
    
    struct TEvEnd : NActors::TEventLocal<TEvEnd, EvEnd> {};
};