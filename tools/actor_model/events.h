#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    // Вам нужно самостоятельно сюда добавить все необходимые events в NActors::TEvents::ES_PRIVATE

    enum Events {
        ForTEvDone = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
        ForRequest
    };

    // завершение TReadActor
    struct TEvDone : public NActors::TEventLocal<TEvDone, ForTEvDone> {};

    // отправка делителя TWriteActor'у
    struct TEvWriteValueRequest : public NActors::TEventLocal<TEvWriteValueRequest, ForRequest> {
        int64_t value;
        TEvWriteValueRequest(int64_t value) : value(value) {}
    };

};
