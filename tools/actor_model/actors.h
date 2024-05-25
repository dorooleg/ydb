#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>

THolder<NActors::IActor> CreateReadActor(NActors::TActorSystem* actorSystem, NActors::TActorId maxPrimeActor);
THolder<NActors::IActor> CreateMaximumPrimeDivisorActor(NActors::TActorSystem* actorSystem, NActors::TActorId writeActor);
THolder<NActors::IActor> CreateWriteActor();
std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();