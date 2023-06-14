#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    enum ECustomEvents {
        EvDiscoveryResponse = NActors::TEvents::ES_PRIVATE + 1,
        EvDone
    };

    struct TEvDone : NActors::TEventLocal<TEvDone, EvDone> {};

    struct TEvWriteValueRequest : NActors::TEventLocal<TEvWriteValueRequest, EvDiscoveryResponse> {
        int64_t Value;
        explicit TEvWriteValueRequest(int64_t value) : Value(value) {}
    };
};
