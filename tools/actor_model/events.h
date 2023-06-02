#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    // Вам нужно самостоятельно сюда добавить все необходимые events в NActors::TEvents::ES_PRIVATE
    
    enum EEv {
        EvDiscoveryResponse = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
        EvEnd
    };

    struct TEvDone : NActors::TEventLocal<TEvDone, EvDiscoveryResponse> {}; 	
    
    struct TEvWriteValueRequest : NActors::TEventLocal<TEvWriteValueRequest, EvDiscoveryResponse> {
    	int64_t Value;
    	explicit TEvWriteValueRequest(int64_t value) : Value(value) {}
    };
    struct TEvPoisonPill : NActors::TEventLocal<TEvPoisonPill, EvDiscoveryResponse> {};
};
