#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>
#include <stdint.h>

struct TEvents {
    enum EEv {
        EvWriteValueRequest = EventSpaceBegin(NActors::TEvents::ES_USERSPACE +  1),
        Poison,
        Gone
    };
    
    struct TEvWriteValueRequest : NActors::TEventLocal<TEvWriteValueRequest, EvWriteValueRequest>
    {
        int64_t Value;

        TEvWriteValueRequest(int64_t &value)
        : Value(value)
        {}
        
    };
    

    struct TEvDone: public NActors::TEventBase<TEvDone, EEv::Gone> {
            DEFINE_SIMPLE_LOCAL_EVENT(TEvDone, "System: TEvGone")
        };

    // Вам нужно самостоятельно сюда добавить все необходимые events в NActors::TEvents::ES_PRIVATE
};