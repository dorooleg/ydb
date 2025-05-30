#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    enum ECustomEvent : ui32 {
        EvWriteRequest = NActors::TEvents::ES_PRIVATE + 1,
        EvComputationDone,
    };

    struct TEvWriteRequest
        : NActors::TEventLocal<TEvWriteRequest, EvWriteRequest>
    {
        int64_t PrimeDivisor;
        explicit TEvWriteRequest(int64_t value)
            : PrimeDivisor(value)
        {}
    };

    struct TEvComputationDone
        : NActors::TEventLocal<TEvComputationDone, EvComputationDone>
    {
        bool Final;
        explicit TEvComputationDone(bool isFinal = false)
            : Final(isFinal)
        {}
    };
};
