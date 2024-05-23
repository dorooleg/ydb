#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    // Вам нужно самостоятельно сюда добавить все необходимые events в NActors::TEvents::ES_PRIVATE
            enum {
                Done = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
  
        };

        struct TEvDone: public NActors::TEventLocal<TEvDone, TEvents::Done> {
            DEFINE_SIMPLE_LOCAL_EVENT(TEvDone, "System: TEvDone")
        };
};
