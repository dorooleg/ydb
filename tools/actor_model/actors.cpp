#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/hfunc.h>
#include <iostream>
#include <chrono> 


static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();


class TWriteActor : public NActors::TActor<TWriteActor> {
    long long CurrentSum_ = 0;

public:
    static constexpr char ActorName[] = "TWriteActor";

    TWriteActor() : NActors::TActor<TWriteActor>(&TWriteActor::StateWork) {}

    STRICT_STFUNC(StateWork, {
        hFunc(TEvents::TEvWriteData, HandleWriteData);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    });

    void HandleWriteData(TEvents::TEvWriteData::TPtr& ev) {
        CurrentSum_ += ev->Get()->Value;
    }

    void HandlePoisonPill() {
        std::cout << CurrentSum_ << std::endl;
        ShouldContinue->ShouldStop(); // Сигнализируем main, что можно завершаться
        PassAway();                   // Актор завершает свою работу
    }
};


THolder<NActors::IActor> CreateWriteActor() {
    return MakeHolder<TWriteActor>();
}


class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    const NActors::TActorId ReadActorId_;
    const NActors::TActorId WriteActorId_;
    const int64_t OriginalValue_;

    int64_t NumberToFactor_;
    int64_t CurrentDivisor_;
    int64_t LargestPrimeFactor_;
    bool IsFirstWakeup_ = true;

    static constexpr TDuration MAX_CALCULATION_SLICE_ = TDuration::MilliSeconds(10);

public:
    static constexpr char ActorName[] = "TMaximumPrimeDevisorActor";

    TMaximumPrimeDevisorActor(int64_t value, NActors::TActorId readActorId, NActors::TActorId writeActorId)
        : ReadActorId_(readActorId)
        , WriteActorId_(writeActorId)
        , OriginalValue_(value)
    {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateWork);
        Send(SelfId(), MakeHolder<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateWork, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
        if (IsFirstWakeup_) {
            NumberToFactor_ = OriginalValue_;
            LargestPrimeFactor_ = 0;
            CurrentDivisor_ = 2;
            IsFirstWakeup_ = false;

            if (OriginalValue_ <= 1) {
                int64_t value_to_send = 0; 
                if (OriginalValue_ == 1) {
                    value_to_send = 1;
                }
                Send(WriteActorId_, MakeHolder<TEvents::TEvWriteData>(value_to_send));
                Send(ReadActorId_, MakeHolder<TEvents::TEvDone>());
                PassAway();
                return;
            }
        }

        auto startTime = TInstant::Now();
        bool calculationDone = false;

        while (true) {
            if (NumberToFactor_ == 1) {
                calculationDone = true;
                break;
            }

            if (CurrentDivisor_ * CurrentDivisor_ > NumberToFactor_) {
                if (NumberToFactor_ > 1) {
                    LargestPrimeFactor_ = NumberToFactor_;
                }
                calculationDone = true;
                break;
            }

            if (NumberToFactor_ % CurrentDivisor_ == 0) {
                LargestPrimeFactor_ = CurrentDivisor_;
                while (NumberToFactor_ % CurrentDivisor_ == 0) {
                    NumberToFactor_ /= CurrentDivisor_;
                }

                if (NumberToFactor_ == 1) {
                    calculationDone = true;
                    break;
                }
            }
            

            if (CurrentDivisor_ == 2) {
                CurrentDivisor_ = 3;
            } else {
                CurrentDivisor_ += 2;
            }


            if (TInstant::Now() - startTime > MAX_CALCULATION_SLICE_) {
                break;
            }
        }
        
        if (calculationDone) {
            if (LargestPrimeFactor_ == 0 && OriginalValue_ > 1) {
                LargestPrimeFactor_ = OriginalValue_;
            }
            Send(WriteActorId_, MakeHolder<TEvents::TEvWriteData>(LargestPrimeFactor_));
            Send(ReadActorId_, MakeHolder<TEvents::TEvDone>());
            PassAway();
        } else {
            Send(SelfId(), MakeHolder<NActors::TEvents::TEvWakeup>());
        }
    }
};


THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t value, NActors::TActorId readActorId, NActors::TActorId writeActorId) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, readActorId, writeActorId);
}



class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    const NActors::TActorId WriteActorId_;
    int PendingDevisorActors_ = 0;
    bool EofReached_ = false;

public:
    static constexpr char ActorName[] = "TReadActor";

    TReadActor(NActors::TActorId writeActorId)
        : WriteActorId_(writeActorId)
    {}

    void Bootstrap() {
        Become(&TReadActor::StateRead);
        Send(SelfId(), MakeHolder<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateRead, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        hFunc(TEvents::TEvDone, HandleDone);
    });

    void HandleWakeup() {
        if (EofReached_) { 
            CheckCompletion();
            return;
        }

        int64_t value;
        if (std::cin >> value) {
            Register(CreateMaximumPrimeDevisorActor(value, SelfId(), WriteActorId_).Release());
            PendingDevisorActors_++;
            Send(SelfId(), MakeHolder<NActors::TEvents::TEvWakeup>()); 
        } else {
            EofReached_ = true;
            CheckCompletion();
        }
    }

    void HandleDone(TEvents::TEvDone::TPtr& /*ev*/) {
        PendingDevisorActors_--;
        Y_VERIFY(PendingDevisorActors_ >= 0, "PendingDevisorActors_ count cannot be negative.");
        CheckCompletion();
    }

    void CheckCompletion() {
        if (EofReached_ && PendingDevisorActors_ == 0) {
            Send(WriteActorId_, MakeHolder<NActors::TEvents::TEvPoisonPill>());
            PassAway();
        }
    }
};


THolder<NActors::IActor> CreateReadActor(NActors::TActorId writeActorId) {
    return MakeHolder<TReadActor>(writeActorId);
}


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
