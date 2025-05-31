#pragma once
#include <library/cpp/actors/core/event_local.h>

namespace NActors {
    struct TEvDone : TEventLocal<TEvDone, 1> {};
    struct TEvWriteValueRequest : TEventLocal<TEvWriteValueRequest, 2> {
        int64_t Value;
        TEvWriteValueRequest(int64_t value) : Value(value) {}
    };
}