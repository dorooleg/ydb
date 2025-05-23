#pragma once

#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents
{
    enum EvTypes : ui32
    {
        EvDataValueRequest = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
        EvDone
    };


    struct TEvWriteValueRequest  : NActors::TEventLocal<TEvWriteValueRequest , EvDataValueRequest>
    {
        int64_t Data;

        TEvWriteValueRequest (int64_t data) : Data(data) {}
    };

    struct TEvDone : NActors::TEventLocal<TEvDone, EvDone>
    {
    };
};
