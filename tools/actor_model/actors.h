#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <library/cpp/actors/util/should_continue.h>

THolder<NActors::IActor> CreateSelfPingActor(const TDuration &latency);

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();

THolder<NActors::IActor> CreateReadActor(const NActors::TActorId &writeActorId);
THolder<NActors::IActor> CreateWriteActor();
THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(
    int64_t value,
    const NActors::TActorId &readActorId,
    const NActors::TActorId &writeActorId);
