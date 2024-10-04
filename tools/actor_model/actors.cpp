#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <chrono>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

int64_t ComputeLargestPrimeDivisor(int64_t number) {
    if (number <= 1)
        return 1;

    int64_t maxPrime = -1;

    while (number % 2 == 0) {
        maxPrime = 2;
        number /= 2;
    }

    for (int64_t i = 3; i * i <= number; i += 2) {
        while (number % i == 0) {
            maxPrime = i;
            number /= i;
        }
    }

    if (number > 2)
        maxPrime = number;

    if (maxPrime == -1)
        maxPrime = number;

    return maxPrime;
}

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    NActors::TActorId WriteActorId;
    int ActiveCalculations = 0;
    bool IsReadingFinished = false;

public:
    TReadActor(const NActors::TActorId& writeActorId)
        : WriteActorId(writeActorId) {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    void HandleWakeup() {
        int64_t inputValue;
        if (std::cin >> inputValue) {
            Register(CreateMaximumPrimeDivisorActor(inputValue, SelfId(), WriteActorId).Release());
            ActiveCalculations++;
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
        } else {
            IsReadingFinished = true;
            if (ActiveCalculations == 0) {
                Send(WriteActorId, new NActors::TEvents::TEvPoisonPill());
                PassAway();
            }
        }
    }

    void HandleCalculationDone() {
        ActiveCalculations--;
        if (IsReadingFinished && ActiveCalculations == 0) {
            Send(WriteActorId, new NActors::TEvents::TEvPoisonPill());
            PassAway();
        }
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        cFunc(TEvents::TEvDone::EventType, HandleCalculationDone);
    });
};

class TMaximumPrimeDivisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDivisorActor> {
    int64_t NumberToProcess;
    NActors::TActorId ReaderActorId;
    NActors::TActorId WriterActorId;
    int64_t CurrentFactor = 2;
    int64_t LargestPrimeDivisor = 1;

public:
    TMaximumPrimeDivisorActor(int64_t number, const NActors::TActorId& readerId, const NActors::TActorId& writerId)
        : NumberToProcess(number), ReaderActorId(readerId), WriterActorId(writerId) {}

    void Bootstrap() {
        Become(&TMaximumPrimeDivisorActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    void HandleWakeup() {
        auto startTime = std::chrono::steady_clock::now();

        if (NumberToProcess <= 1) {
            LargestPrimeDivisor = 1;
        } else {
            while (NumberToProcess >= CurrentFactor && CurrentFactor * CurrentFactor <= NumberToProcess) {
                if (NumberToProcess % CurrentFactor == 0) {
                    LargestPrimeDivisor = CurrentFactor;
                    NumberToProcess /= CurrentFactor;
                } else {
                    CurrentFactor++;
                }

                auto currentTime = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
                if (elapsed >= 10) {
                    Send(SelfId(), new NActors::TEvents::TEvWakeup());
                    return;
                }
            }

            if (NumberToProcess > 1) {
                LargestPrimeDivisor = NumberToProcess;
            }
        }

        Send(WriterActorId, new TEvents::TEvWriteValueRequest(LargestPrimeDivisor));
        Send(ReaderActorId, new TEvents::TEvDone());
        PassAway();
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });
};

class TWriteActor : public NActors::TActor<TWriteActor> {
    int64_t TotalSum = 0;

public:
    TWriteActor() : TActor(&TWriteActor::StateFunc) {}

    void HandleWriteRequest(TEvents::TEvWriteValueRequest::TPtr& ev) {
        TotalSum += ev->Get()->Value;
    }

    void HandlePoisonPill() {
        std::cout << TotalSum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandleWriteRequest);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    });
};

THolder<NActors::IActor> CreateReadActor(const NActors::TActorId& writeActorId) {
    return MakeHolder<TReadActor>(writeActorId);
}

THolder<NActors::IActor> CreateMaximumPrimeDivisorActor(int64_t number, const NActors::TActorId& readerId, const NActors::TActorId& writerId) {
    return MakeHolder<TMaximumPrimeDivisorActor>(number, readerId, writerId);
}

THolder<NActors::IActor> CreateWriteActor() {
    return MakeHolder<TWriteActor>();
}

class TSelfPingActor : public NActors::TActorBootstrapped<TSelfPingActor> {
    TDuration Latency;
    TInstant LastPingTime;

public:
    TSelfPingActor(const TDuration& latency)
        : Latency(latency) {}

    void Bootstrap() {
        LastPingTime = TInstant::Now();
        Become(&TSelfPingActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    void HandleWakeup() {
        auto now = TInstant::Now();
        Y_VERIFY(now - LastPingTime <= Latency, "Latency exceeded the threshold");
        LastPingTime = now;
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });
};

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency) {
    return MakeHolder<TSelfPingActor>(latency);
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
