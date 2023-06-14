#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor : public NActors::TActorBootstrapped {
public:
    TReadActor(std::istream& strm, NActors::TActorId writeActor)
        : strm_(strm), writeActor_(writeActor) {}

    void Bootstrap(NActors::TActorContext& context) override {
        send(context.Self(), NActors::TEvents::TEvWakeup);
    }

    void Receive(NActors::TActorContext& context, const NActors::TEvent& event) override {
        if (event.IsA<NActors::TEvents::TEvWakeup>()) {
            int64_t value;
            if (strm_ >> value) {
                registerActor(NActors::TActorFactory::Create<TMaximumPrimeDevisorActor>(value, context.Self(), context.Sender()));
                send(context.Self(), NActors::TEvents::TEvWakeup);
            } else {
                ConstSend(writeActor_, NActors::TEvents::TEvPoisonPill());
            }
        } else if (event.IsA<NActors::TEvents::TEvDone>()) {
            numFinished_++;
            if (numFinished_ == numActors_) {
                ConstSend(writeActor_, NActors::TEvents::TEvPoisonPill());
            }
        }
    }

private:
    std::istream& strm_;
    NActors::TActorId writeActor_;
    size_t numActors_ = 0;
    size_t numFinished_ = 0;

    void registerActor(NActors::TActor<TMaximumPrimeDevisorActor>& actor) {
        actor.Send(NActors::TEvents::TEvWakeup());
        numActors_++;
    }
};


class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    const int64_t Value;
    const NActors::TActorId ReadActor;
    const NActors::TActorId WriteActor;
    bool Calculating;
    std::vector<int64_t> PrimeDivisors;

public:
    TMaximumPrimeDevisorActor(int64_t value, NActors::TActorId readActor, NActors::TActorId writeActor)
        : Value(value), ReadActor(readActor), WriteActor(writeActor), Calculating(false)
    {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Calculating = true;
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
        cFunc(NActors::TEvents::TEvCalculationTimeout::EventType, HandleTimeout);
    });

    void HandleWakeUp() {
        auto tStart = std::chrono::high_resolution_clock::now();
        CalculatePrimeDivisors();

        auto tEnd = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart).count();
        if (elapsed >= 10) {
            WarnL << "Calculation timed out";
            Calculating = true;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            Send(WriteActor, std::make_unique<TEvents::TEvWriteValueRequest>(SelfId(), PrimeDivisors.back()));
            Send(ReadActor, std::make_unique<TEvents::TEvMaximumPrimeDevisorFound>(PrimeDivisors.back()));
            PassAway();
        }
    }
   void HandleTimeout(const NActors::TEventPtr& event) {
        WarnL << "Calculation timed out";
        Calculating = true;
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

   void CalculatePrimeDivisors() {
        for (int64_t i = 2; i * i <= Value; ++i) {
            if (Value % i == 0) {
                PrimeDivisors.push_back(i);
                while (Value % i == 0) Value /= i;
            }
        }
        if (Value != 1) PrimeDivisors.push_back(Value);
        int64_t maximumPrimeDivisor = *std::max_element(PrimeDivisors.begin(), PrimeDivisors.end());
        Send(WriteActor, std::make_unique<NActors::TEvents::TEvWriteValueRequest>(SelfId(), maximumPrimeDivisor));
        Send(ReadActor, std::make_unique<NActors::TEvents::TEvMaximumPrimeDevisorFound>(maximumPrimeDivisor));
        Calculating = false;
    }
};

class TWriteActor : public NActors::TActor {
    int64_t Sum;
public:
    TWriteActor() : Sum(0) {}

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
        cFunc(TEvents::TEvWriteValueRequest::EventType, HandleWriteValueRequest);
    });

    void HandlePoisonPill(const NActors::TEventPtr& event) {
        std::cout << Sum << std::endl;
        ShouldStop();
        PassAway();
    }

    void HandleWriteValueRequest(const NActors::TEventPtr& event) {
        auto value = event->As<TEvents::TEvWriteValueRequest>()->Value;
        Sum += value;
    }
};

class TSelfPingActor : public NActors::TActorBootstrapped<TSelfPingActor> {
    TDuration Latency;
    TInstant LastTime;

public:
    TSelfPingActor(const TDuration& latency)
        : Latency(latency)
    {}

    void Bootstrap() {
        LastTime = TInstant::Now();
        Become(&TSelfPingActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
        auto now = TInstant::Now();
        TDuration delta = now - LastTime;
        Y_VERIFY(delta <= Latency, "Latency too big");
        LastTime = now;
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }
};

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency) {
    return MakeHolder<TSelfPingActor>(latency);
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
