#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvWriteValueRequest : public NActors::TEventLocal<TEvWriteValueRequest, 0> {
    int64_t Value;
    TEvWriteValueRequest(int64_t value) : Value(value) {}
};

struct TEvDone : public NActors::TEventLocal<TEvDone, 1> {};
