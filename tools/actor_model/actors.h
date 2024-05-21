#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();

THolder<NActors::IActor> CreateReadActor(NActors::TActorId writing);

THolder<NActors::IActor> CreateMaxPrimeDevActor(NActors::TActorIdentity read_actor, NActors::TActorId write_actor, int64_t value);

THolder<NActors::IActor> CreateWriteActor();
