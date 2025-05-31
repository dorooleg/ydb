// events.h
#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    enum EEv {
        EvWriteValueRequest = NActors::TEvents::ES_PRIVATE, // Событие для передачи значения
        EvDone, // Событие завершения вычислений
        EvCompute // Событие для продолжения вычислений
    };

    // Событие с вычисленным значением
    struct TEvWriteValueRequest : NActors::TEventLocal<TEvWriteValueRequest, EvWriteValueRequest> {
        int64_t Value;
        explicit TEvWriteValueRequest(int64_t value) : Value(value) {}
    };

    // Событие завершения работы
    struct TEvDone : NActors::TEventLocal<TEvDone, EvDone> {};

    // Событие для продолжения вычислений
    struct TEvCompute : NActors::TEventLocal<TEvCompute, EvCompute> {};
};