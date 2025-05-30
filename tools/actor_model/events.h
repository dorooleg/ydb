#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    struct EventIdentifiers {
        static constexpr int RESULT_EVENT = EventSpaceBegin(NActors::TEvents::ES_PRIVATE);
        static constexpr int COMPLETION_EVENT = RESULT_EVENT + 1;
    };

    struct TWriteResultEvent : public NActors::TEventLocal<TWriteResultEvent, EventIdentifiers::RESULT_EVENT> {
        int64_t ResultValue;

        TWriteResultEvent(int64_t value) : ResultValue(value) {}
    };

    struct TComputationDone : public NActors::TEventLocal<TComputationDone, EventIdentifiers::COMPLETION_EVENT> {
        static constexpr const char* DESCRIPTION = "ComputationDoneEvent";
    };
};
