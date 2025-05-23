#pragma once

#include <library/cpp/actors/core/event_local.h>

struct TEvents {
  // Перечисление типов событий
  enum EEv : ui32 {
    EvWriteValueRequest = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
    EvDone
  };

  // Структура события для передачи значения на запись
  struct TEvWriteValueRequest
      : NActors::TEventLocal<TEvWriteValueRequest, EvWriteValueRequest> {
    int64_t Value;
    explicit TEvWriteValueRequest(int64_t value) : Value(value) {}
  };

  // Структура события для сигнализации о завершении работы актора
  struct TEvDone : NActors::TEventLocal<TEvDone, EvDone> {};
};