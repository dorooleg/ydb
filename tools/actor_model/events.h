#pragma once

#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

namespace MyActorModel {

struct TEvents {
  enum EEv : ui32 {
    EvWriteValueRequest = NActors::TEvents::ES_PRIVATE,
    EvDone,
  };

  struct TEvWriteValueRequest
      : NActors::TEventLocal<TEvWriteValueRequest, EvWriteValueRequest> {
    int64_t Value;
    explicit TEvWriteValueRequest(int64_t v) : Value(v) {}
  };

  struct TEvDone : NActors::TEventLocal<TEvDone, EvDone> {};
};

}