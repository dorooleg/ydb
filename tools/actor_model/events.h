#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    struct Events {
        static constexpr int Done = EventSpaceBegin(NActors::TEvents::ES_PRIVATE);
        static constexpr int WriteValueRequest = Done + 1;
    };

    struct TEvDone : public NActors::TEventLocal<TEvDone, Events::Done> {
        static constexpr const char* NAME = "MyEvent::Done";
    };

    struct TEvWriteValueRequest : public NActors::TEventLocal<TEvWriteValueRequest, Events::WriteValueRequest> {
        int64_t value;

        TEvWriteValueRequest(int64_t value) : value(value) {}
    };
};