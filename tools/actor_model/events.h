#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

namespace CustomEvents {
    enum EEvCustom {
        EvPrimeFactor = NActors::TEvents::ES_PRIVATE + 1,
        EvHandleInput = NActors::TEvents::ES_PRIVATE + 2
    };

    struct TEvFactorFound : public NActors::TEventLocal<TEvFactorFound, EvPrimeFactor> {
        int Value;
        explicit TEvFactorFound(int value) : Value(value) {}
    };

    struct TEvHandleInput : public NActors::TEventLocal<TEvHandleInput, EvHandleInput> {
        int Input;
        explicit TEvHandleInput(int input) : Input(input) {}
    };
}