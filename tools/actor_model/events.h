#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    // Вам нужно самостоятельно сюда добавить все необходимые events в NActors::TEvents::ES_PRIVATE
    
    enum EEv {
        EvDiscoveryResponse = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
        EvEnd
    };

    static_assert(EvEnd < EventSpaceEnd(NActors::TEvents::ES_PRIVATE), "expect EvEnd < EventSpaceEnd(TEvents::ES_PRIVATE)");
  
    struct TEvDone : NActors::TEventLocal<TEvDone, EvDiscoveryResponse> {}; 	
    
    struct TEvWriteValueRequest : NActors::TEventLocal<TEvWriteValueRequest, EvDiscoveryResponse> {
    	int Value;
    };
    struct TEvPoisonPill {};
};
