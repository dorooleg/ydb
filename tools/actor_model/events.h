#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    struct TEventFinish : public NActors::TEventLocal<TEventFinish, NActors::TEvents::ES_PRIVATE> {};
    struct TEventWriteValueRequest : public NActors::TEventLocal<TEventWriteValueRequest, NActors::TEvents::ES_PRIVATE> {
        int64_t Value;
        TEventWriteValueRequest(int64_t value) : Value(value) {}
    };
};
