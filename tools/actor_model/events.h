#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    enum EEv {
        EvDiscoveryResponse = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
        EvDone
    };

    struct TEvDone : NActors::TEventLocal<TEvDone, EvDone> {};
    
    struct TEvWriteValueRequest : NActors::TEventLocal<TEvWriteValueRequest, EvDiscoveryResponse> {
    	int64_t Value;
    	explicit TEvWriteValueRequest(int64_t value) : Value(value) {}
    };
};