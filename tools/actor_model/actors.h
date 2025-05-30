#ifndef ACTORS_H
#define ACTORS_H

#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>
#include <iostream>

namespace NPrimeDivisors {

// Объявление вспомогательного класса для проверки простых чисел
class TPrimeChecker {
public:
    struct TCheckResult {
        bool IsCompleted;
        bool IsPrime;
        int LastChecked;
    };

    static TCheckResult CheckWithTimeout(int number, int startFrom, 
                                       const std::chrono::steady_clock::time_point& startTime);
};

// Объявления акторов
class TMaxPrimeDivisorFinder : public NActors::TActorBootstrapped<TMaxPrimeDivisorFinder> {
public:
    TMaxPrimeDivisorFinder(int64_t number, const NActors::TActorId& sender, 
                          const NActors::TActorId& receiver);
};

class TInputReader : public NActors::TActorBootstrapped<TInputReader> {
public:
    TInputReader(const NActors::TActorId& writer);
};

class TResultWriter : public NActors::TActorBootstrapped<TResultWriter> {
public:
    TResultWriter();
};

class TLatencyTester : public NActors::TActorBootstrapped<TLatencyTester> {
public:
    TLatencyTester(const TDuration& latency);
};

} // namespace NPrimeDivisors

// Фабричные функции
THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);
THolder<NActors::IActor> CreateReadActor(IInputStream& strm, const NActors::TActorId& writeActorId);
THolder<NActors::IActor> CreateMaximumPrimeDivisorActor(int64_t value, 
                                                      const NActors::TActorId& readActorId,
                                                      const NActors::TActorId& writeActorId);
THolder<NActors::IActor> CreateWriteActor();

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();

#endif // ACTORS_H