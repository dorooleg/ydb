#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

#define EventSpaceBegin(eventSpace) (eventSpace << 16u)
#define EventSpaceEnd(eventSpace) ((eventSpace << 16u) + (1u << 16u))

struct TEvents {
    // Вам нужно самостоятельно сюда добавить все необходимые events в NActors::TEvents::ES_PRIVATE
    enum EEventSpace {
        ES_PRIVATE = (1 << 15) - 16
    };

    struct THelloWorld {
        enum {
            Start = EventSpaceBegin(ES_PRIVATE),
            Done,
            WriteValueRequest,
            End
        };
        static_assert(End < EventSpaceEnd(ES_PRIVATE), "expect End < EventSpaceEnd(ES_PRIVATE)");
    };

    struct TEvDone : public NActors::TEventLocal<TEvDone, THelloWorld::Done>{
    };

    struct TEvWriteValueRequest : public NActors::TEventLocal<TEvWriteValueRequest, THelloWorld::WriteValueRequest>{
        const int64_t Value;
        TEvWriteValueRequest(const int64_t value)
            : Value(value)
        {}
    };
    
};
