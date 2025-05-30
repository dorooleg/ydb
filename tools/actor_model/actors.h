#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>

THolder<NActors::IActor> CreateHeartbeatActor(const TDuration& interval);
THolder<NActors::IActor> CreateInputReader(std::istream& inputStream, NActors::TActorId outputActor);
THolder<NActors::IActor> CreateOutputWriter();
std::shared_ptr<TProgramShouldContinue> GetContinueFlag();
