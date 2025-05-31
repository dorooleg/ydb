#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>
THolder<NActors::IActor> CreateReadActor(NActors::TActorId writer);
NActors::IActor* CreateMaximumPrimeDivisorActor(int64_t value, NActors::TActorId reader, NActors::TActorId writer);
THolder<NActors::IActor> CreateWriteActor();
THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();