#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);
THolder<NActors::IActor> CreateSelfTReadActor(NActors::TActorId writeActor);
THolder<NActors::IActor> CreateSelfTMaximumPrimeDevisorActor(int64_t value, NActors::TActorId readActor, NActors::TActorId writeActor);
THolder<NActors::IActor> CreateSelfTWriteActor();

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();
