#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>

THolder<NActors::IActor> CreateSumHandler();
THolder<NActors::IActor> CreateInputHandler(NActors::TActorSystem* actorSystem, NActors::TActorId primeActor);
THolder<NActors::IActor> CreatePrimeHandler(NActors::TActorSystem* actorSystem, NActors::TActorId sumActor);
std::shared_ptr<TProgramShouldContinue> GetProgramStatus();