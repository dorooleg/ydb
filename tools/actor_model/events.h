#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    enum EEv {
        EvDone = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
        EvWriteValueRequest
    };

    struct TEvDone : NActors::TEventLocal<TEvDone, EvDone> {};
    struct TEvWriteValueRequest : NActors::TEventLocal<TEvWriteValueRequest, EvWriteValueRequest> {
        int64_t Value;
        TEvWriteValueRequest(int64_t value) : Value(value) {}
    };
};
