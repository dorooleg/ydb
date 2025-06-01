#pragma once

#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h> // Для NActors::TEvents::ES_PRIVATE

struct TEvents {
    // События, передаваемые между акторами
    enum EEv {
        EvDone = NActors::TEvents::ES_PRIVATE + 1,      // От TMaximumPrimeDevisorActor к TReadActor
        EvWriteData = NActors::TEvents::ES_PRIVATE + 2   // От TMaximumPrimeDevisorActor к TWriteActor
    };

    // Событие, информирующее о завершении вычисления для одного числа
    // Отправляется: TMaximumPrimeDevisorActor
    // Получатель: TReadActor
    struct TEvDone : NActors::TEventLocal<TEvDone, EEv::EvDone> {
        TEvDone() = default;
    };

    // Событие, передающее вычисленный наибольший простой делитель
    // Отправляется: TMaximumPrimeDevisorActor
    // Получатель: TWriteActor
    struct TEvWriteData : NActors::TEventLocal<TEvWriteData, EEv::EvWriteData> {
        int64_t Value; // Посчитанное значение
        TEvWriteData(int64_t value) : Value(value) {}
    };
};
