#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);

THolder<NActors::IActor> CreateReadActor(std::istream& inputStream, NActors::TActorId WriteActor);

THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t Value, const NActors::TActorId ReadActor, const NActors::TActorId WriteActor);

THolder<NActors::IActor> CreateWriteActor();

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();
