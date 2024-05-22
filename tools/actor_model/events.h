#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    /*Вам нужно самостоятельно сюда добавить все 
    необходимые events в NActors::TEvents::ES_PRIVATE*/
    struct Events {
        static constexpr int WRITE_VALUE_REQUEST = EventSpaceBegin(NActors::TEvents::ES_PRIVATE);
        static constexpr int DONE = WRITE_VALUE_REQUEST + 1;
    };

    struct TEvWriteValueRequest : public NActors::TEventLocal<TEvWriteValueRequest, Events::WRITE_VALUE_REQUEST> {
        int64_t value;

        TEvWriteValueRequest(int64_t value) : value(value) {}
    };

    struct TEvDone : public NActors::TEventLocal<TEvDone, Events::DONE> {
        static constexpr const char* NAME = "MyEvent::DONE";
    };
};
