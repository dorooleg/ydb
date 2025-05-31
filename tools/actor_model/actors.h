#pragma once
#include <library/cpp/actors/core/actor.h>
#include <library/cpp/actors/core/actor_bootstrapped.h>

NActors::IActor* CreateReadActor(NActors::TActorId maxActor);
NActors::IActor* CreateMaxPrimeActor(NActors::TActorId writeActor);
NActors::IActor* CreateWriteActor();
