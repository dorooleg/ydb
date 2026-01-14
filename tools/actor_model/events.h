#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    enum EEv {
        EvDone = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
        EvWriteValueRequest,
        EvEnd
    };

    struct TEvDone : public NActors::TEventLocal<TEvDone, EvDone> {};

    struct TEvWriteValueRequest : public NActors::TEventLocal<TEvWriteValueRequest, EvWriteValueRequest> {
        i64 Value;

        TEvWriteValueRequest(i64 value)
            : Value(value)
        {}
    };
};
