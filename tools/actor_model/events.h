#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    struct TEvWriteValueRequest : public NActors::TEventLocal<TEvWriteValueRequest, 1> {
        int64_t Value;
        TEvWriteValueRequest(int64_t value = 0) : Value(value) {}
    };

    struct TEvDone : public NActors::TEventLocal<TEvDone, 2> {
        TEvDone() = default;
    };
};