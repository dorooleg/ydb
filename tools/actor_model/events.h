#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

// объявляем наши события на базе событий акторной системы
struct TEvents {
    // создаем уникальный тип события для отправки значения между акторами
    struct TEvPrimeNumber : public NActors::TEventLocal<TEvPrimeNumber, NActors::TEvents::ES_PRIVATE> {
        int64_t Number;

        explicit TEvPrimeNumber(int64_t number) : Number(number) {}
    };

    // событие для передачи результатов расчета в TWriteActor
    struct TEvWriteValueRequest : public NActors::TEventLocal<TEvWriteValueRequest, NActors::TEvents::ES_PRIVATE> {
        int64_t Value;

        explicit TEvWriteValueRequest(int64_t value) : Value(value) {}
    };

    // событие для сигнализации о завершении работы ReadActor
    struct TEvDone : public NActors::TEventLocal<TEvDone, NActors::TEvents::ES_PRIVATE> {};
};
