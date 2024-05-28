#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    // Вам нужно самостоятельно сюда добавить все необходимые events в NActors::TEvents::ES_PRIVATE

    enum {
        EvDone = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
        EvWriteValueRequest
        //EvPoisonPill - in ydb\library\cpp\actors\core\events.h
        //EvWakeup - in ydb\library\cpp\actors\core\events.h
    };

    struct TEvDone : NActors::TEventLocal<TEvDone, EvDone> {
        TEvDone() = default;
    };
    struct TEvWriteValueRequest : NActors::TEventLocal<TEvWriteValueRequest, EvWriteValueRequest> {
    	int64_t value;
		TEvWriteValueRequest(int64_t value) : value(value) { }
    };  
};
