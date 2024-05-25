#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);
THolder<IActor> CreateReadActor(TActorId maxPrimeDevisorActor);
THolder<IActor> CreateMaximumPrimeDevisorActor(int64_t number, TActorId readActorId, TActorId writeActorId);
THolder<IActor> CreateWriteActor();

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();
