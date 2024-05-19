#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    // Вам нужно самостоятельно сюда добавить все необходимые events в NActors::TEvents::ES_PRIVATE
    enum EEv {
        EvDiscoveryResponse = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
        EvEnd
    };

    // Event to signal completion
    struct TEvDone : NActors::TEventLocal<TEvDone, EvEnd> {};

    // Event for requesting to write a value
    struct TEvWriteValueRequest : NActors::TEventLocal<TEvWriteValueRequest, EvDiscoveryResponse> {
        int64_t value;  // Value to be written

        // Constructor to initialize the value
        explicit TEvWriteValueRequest(int64_t value)
            : value(value) {}
    };
};
