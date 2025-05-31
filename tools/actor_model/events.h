#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TMyEvents {
    enum {
        EvSendValue = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
        EvComplete,
    };
};

struct TEvSendValue : NActors::TEventLocal<TEvSendValue, TMyEvents::EvSendValue> {
    const int64_t Value;
    explicit TEvSendValue(int64_t value) : Value(value) {}
};

struct TEvComplete : NActors::TEventLocal<TEvComplete, TMyEvents::EvComplete> {};