#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    // Вам нужно самостоятельно сюда добавить все необходимые events в NActors::TEvents::ES_PRIVATE
    enum {
        EvWriterMessage = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
        EvDone
    };

    struct TEvWriteValueRequest : NActors::TEventLocal<TEvWriteValueRequest, EvWriterMessage> {
    	int64_t value;
    	explicit TEvWriteValueRequest(int64_t value) : value(value) {}
    };

    struct TEvDone : NActors::TEventLocal<TEvDone, EvDone> {};
};