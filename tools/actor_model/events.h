#pragma once
#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    enum EEv {
        EvWriteValueRequest = NActors::TEvents::ES_PRIVATE,
        EvDone,
    };

    struct TEvWriteValueRequest
        : public NActors::TEventLocal<TEvWriteValueRequest, EvWriteValueRequest>
    {
        ui64 Value;
        explicit TEvWriteValueRequest(ui64 v) : Value(v) {}
    };

    struct TEvDone
        : public NActors::TEventLocal<TEvDone, EvDone>
    {};
};