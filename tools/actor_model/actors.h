#pragma once
#include <iostream>
#include <memory>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);
std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();

THolder<NActors::IActor> CreateReadActor(NActors::TActorId writeActor);
THolder<NActors::IActor> CreateWriteActor();

class TMaximumPrimeDevisorActor;