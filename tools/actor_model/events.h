#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    // Вам нужно самостоятельно сюда добавить все необходимые events в NActors::TEvents::ES_PRIVATE
    struct MyEvents {
        enum {
            Done = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
            WriteValueRequest
        };
    };

    struct TEvDone: public NActors::TEventLocal<TEvDone, MyEvents::Done> {
        DEFINE_SIMPLE_LOCAL_EVENT(TEvDone, "MyEvent::Done");
    };

    struct TEvWriteValueRequest: public NActors::TEventLocal<TEvWriteValueRequest, MyEvents::WriteValueRequest> {
        int64_t value;

        TEvWriteValueRequest(int64_t value) {
            this->value = value;
        }
    };
};
