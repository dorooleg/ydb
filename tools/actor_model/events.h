#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    static const ui32 EvMaxPrimeDivisor = NActors::TEvents::ES_PRIVATE + 1;
    static const ui32 EvProcessNumber = NActors::TEvents::ES_PRIVATE + 2;

    struct TEvPrimeDivisorFound : public NActors::TEventLocal<TEvPrimeDivisorFound, EvMaxPrimeDivisor> {
        int Value;
        explicit TEvPrimeDivisorFound(int value) : Value(value) {}
    };

    struct TEvProcessNumber : public NActors::TEventLocal<TEvProcessNumber, EvProcessNumber> {
        int Number;
        explicit TEvProcessNumber(int number) : Number(number) {}
    };
};