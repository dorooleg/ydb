#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);
THolder<NActors::IActor> CreateTReadActor(const TDuration& latency);
THolder<NActors::IActor> CreateTMaximumPrimeDevisorActor(const auto readActor, int64_t value);

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();
