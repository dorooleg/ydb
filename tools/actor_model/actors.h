#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);

THolder<NActors::IActor> CreateReadActor(std::istream& InputStream, NActors::TActorId writeActor);
THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t value, NActors::TActorId readActor, NActors::TActorId writeActor);
THolder<NActors::IActor> CreateWriteActor();

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();