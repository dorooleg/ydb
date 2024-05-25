#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>

THolder <NActors::IActor> CreateSelfPingActor(const TDuration &latency);

std::shared_ptr <TProgramShouldContinue> GetProgramShouldContinue();

THolder <NActors::IActor> CreateReadActor(std::shared_ptr <TProgramShouldContinue> shouldContinue);

THolder <NActors::IActor> CreateWriteActor(std::shared_ptr <TProgramShouldContinue> shouldContinue);

THolder <NActors::IActor>
CreateMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId &readActor, const NActors::TActorId &writeActor);