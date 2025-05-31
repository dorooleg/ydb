#pragma once
#include <library/cpp/actors/core/events.h>

struct TEvInputNumber : NActors::TEventLocal<TEvInputNumber, 1> {
    int Value;
    TEvInputNumber(int value) : Value(value) {}
};

struct TEvInputFinished : NActors::TEventLocal<TEvInputFinished, 2> {};

struct TEvPrimeDivisor : NActors::TEventLocal<TEvPrimeDivisor, 3> {
    int Value;
    TEvPrimeDivisor(int value) : Value(value) {}
};
