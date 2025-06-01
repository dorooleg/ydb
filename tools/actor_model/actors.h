#pragma once

#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h> // TProgramShouldContinue

// Объявления фабричных функций для ваших акторов
THolder<NActors::IActor> CreateReadActor(NActors::TActorId writeActorId);
THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t value, NActors::TActorId readActorId, NActors::TActorId writeActorId);
THolder<NActors::IActor> CreateWriteActor();

// Объявления изначальных вспомогательных функций (если они не перенесены в actors.cpp)
THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);
std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();