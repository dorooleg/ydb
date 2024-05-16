
#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor : public NActors::TActorBootstrapped {
public:
    TReadActor(std::istream& strm, const NActors::TActorId& writeActor)
            : Strm(strm)
            , WriteActor(writeActor)
    {}

private:
    std::istream& Strm;
    NActors::TActorId WriteActor;
    bool Finish = false;
    size_t DoneCount = 0;

    void Bootstrap() override {
        Send(SelfId(), NActors::TEvents::TEvWakeup());
    }

    void Handle(const NActors::TEvents::TEvWakeup&) {
        int64_t value;
        if (Strm >> value) {
            register(TMaximumPrimeDevisorActor(value, SelfId(), WriteActor));
            Send(SelfId(), NActors::TEvents::TEvWakeup());
        } else {
            Finish = true;
            if (DoneCount == 0) {
                Send(WriteActor, NActors::TEvents::TEvPoisonPill());
                ShouldContinue->Stop();
            }
        }
    }

    void Handle(const NActors::TEvents::TEvDone&) {
        DoneCount++;
        if (Finish && DoneCount == ActiveCount()) {
            Send(WriteActor, NActors::TEvents::TEvPoisonPill());
            ShouldContinue->Stop();
        }
    }
};

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped {
public:
    TMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId& readActor, const NActors::TActorId& writeActor)
            : Value(value)
            , ReadActor(readActor)
            , WriteActor(writeActor)
    {}

private:
    int64_t Value;
    NActors::TActorId ReadActor;
    NActors::TActorId WriteActor;
    int64_t Result = 1;

    void Bootstrap() override {
        Send(SelfId(), NActors::TEvents::TEvWakeup());
    }

    void Handle(const NActors::TEvents::TEvWakeup&) {
        auto start = std::chrono::high_resolution_clock::now();
        for (int64_t i = 2; i <= Value / i; ++i) {
            while (Value % i == 0) {
                Result = i;
                Value /= i;
            }
            if (std::chrono::high_resolution_clock::now() - start > std::chrono::milliseconds(10)) {
                Send(SelfId(), NActors::TEvents::TEvWakeup());
                return;
            }
        }
        if (Value > 1) {
            Result = Value;
        }
        Send(WriteActor, NActors::TEvents::TEvWriteValueRequest(Result));
        Send(ReadActor, NActors::TEvents::TEvDone());
        PassAway();
    }
};

class TWriteActor : public NActors::TActor {
public:
    void Handle(const NActors::TEvents::TEvWriteValueRequest& ev) {
        Sum += ev.Value;
    }

    void Handle(const NActors::TEvents::TEvPoisonPill&) {
        std::cout << Sum << std::endl;
        ShouldContinue->Stop();
        PassAway();
    }

private:
    int64_t Sum = 0;
};