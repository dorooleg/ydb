#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>
  
std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();
THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);

THolder<NActors::IActor> CreateReadActor(NActors::TActorId writer);
THolder<NActors::IActor> CreateWriteActor();

NActors::IActor* CreateMaximumPrimeDivisorActor(int64_t value, NActors::TActorId reader, NActors::TActorId writer);
