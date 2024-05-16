#pragma once

#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>


struct TEvents {

    enum EEvents {
        EvWriteValueRequest = NActors::TEvents::ES_PRIVATE,
        EvProcessingFinished
    };


    struct TEvWriteValueRequest : public NActors::TEventLocal<TEvWriteValueRequest, EvWriteValueRequest> {

        int64_t Value;

        explicit TEvWriteValueRequest(int64_t value)
                : Value(value) {}
    };

    struct TEvProcessingFinished : public NActors::TEventLocal<TEvProcessingFinished, EvProcessingFinished> {};
};