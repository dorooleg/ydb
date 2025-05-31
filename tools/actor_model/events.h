#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    enum EEventType : ui32 {
        EvWriteValueRequest = NActors::TEvents::ES_PRIVATE,
        EvDone
    };

    class TEvWriteValueRequest : public NActors::TEventLocal<TEvWriteValueRequest, EEventType::EvWriteValueRequest> {
    public:
        int64_t Value;

        explicit TEvWriteValueRequest(int64_t value)
            : Value(value)
        {}
    };

    class TEvDone : public NActors::TEventLocal<TEvDone, EEventType::EvDone> {
    public:
        TEvDone() {}
    };
};