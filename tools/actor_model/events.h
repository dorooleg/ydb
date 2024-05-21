#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    enum EEv : ui32 {
        EvWriteValueRequest = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
        EvDone,
        EvEnd
    };

    struct TEvWriteValueRequest : public NActors::TEventLocal<TEvWriteValueRequest, EvWriteValueRequest> {
        int64_t Value;

        TEvWriteValueRequest(int64_t value)
            : Value(value)
        {}
    };

    struct TEvDone : public NActors::TEventLocal<TEvDone, EvDone> {};
};