#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    // Вам нужно самостоятельно сюда добавить все необходимые events в NActors::TEvents::ES_PRIVATE
    enum EEv {
        EvWriteValueRequest = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),//to ensure that these events do not overlap with other events in the system.
        EvDone //notify that a task has been completed.
    };
    struct TEvWriteValueRequest : NActors::TEventLocal<TEvWriteValueRequest, EvWriteValueRequest> {
        int64_t Value;
        explicit TEvWriteValueRequest(int64_t inputValue) : Value(inputValue) {}
    };
    struct TEvDone : NActors::TEventLocal<TEvDone, EvDone> {};
};
