#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);
THolder<NActors::IActor> CreateTReadActor(const NActors::TActorId writeActor);
THolder<NActors::IActor> CreateTMaximumPrimeDevisorActor(int64_t value, const NActors::TActorIdentity readActor, const NActors::TActorId writeActor);
THolder<NActors::IActor> CreateTWriteActor();
std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();
