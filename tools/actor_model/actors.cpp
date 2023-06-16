#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

// TODO: Write TWriteActor implementation

class TWriteActor : public NActors::TActor<TWriteActor> {
    int64_t Sum;
public:
    using TBase = NActors::TActor<TWriteActor>;
    TWriteActor(): TBase(&TWriteActor::StateFunc), Sum(0) // В базовый конструктор передаём функцию обработчик
    {}

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandleWriteValue);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandleDie); // макрос выбора хендлера, не передающий ивент в обработчик
    });

    void HandleWriteValue(TEvents::TEvWriteValueRequest::TPtr &ev) {
        auto& event = *ev->Get();
        Sum = Sum + event.Num;
    }

    void HandleDie() {
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }
};


class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    bool Finish = false;
    const NActors::TActorId WriteActorId;
    int Count;
public:
    TReadActor(const NActors::TActorId writeActorId)
            : Finish(false), WriteActorId(writeActorId), Count(0)
    {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc); // установить функцию-обработчик сообщений, которая будет использована при получении следующего сообщения
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
        cFunc(TEvents::TEvDone::EventType, HandleDone); // макрос выбора хендлера, не передающий ивент в обработчик
    });

    void HandleWakeUp() {
        int64_t value;
        if (std::cin >> value) {
            Register(CreateTMaximumPrimeDevisorActor(value, SelfId(), WriteActorId).Release());
            Count++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            Finish = true;
            if(Count == 0) {
                Send(WriteActorId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
                PassAway();
            }
        }
    }

    void HandleDone() {
        Count--;
        if (Finish && Count == 0) {
            Send(WriteActorId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            PassAway();
        }
    }
};

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t Num;
    int64_t OriginNum;
    const NActors::TActorId ReadActorId;
    const NActors::TActorId WriteActorId;
    int64_t CurrentDivisor = 2;
    int64_t MaxDivisor = 0;
public:
    TMaximumPrimeDevisorActor(int64_t num, const NActors::TActorId readActorId, const NActors::TActorId writeActorId): Num(num),OriginNum(num) ,ReadActorId(readActorId), WriteActorId(writeActorId)
    {
    }

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
    });

    void HandleWakeUp() {
        auto start = TInstant::Now();
        TInstant end;
        TDuration maxDelay = TDuration::MilliSeconds(10);
        for (int64_t i=CurrentDivisor; i <= sqrt(Num); i+=(i==2?1:2)) {
            CurrentDivisor = i;

            while (Num % CurrentDivisor == 0) {
                MaxDivisor = CurrentDivisor;
                Num /= CurrentDivisor;
                end = TInstant::Now();
                if (end - start >= maxDelay) {
                    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                    return;
                }
            }
            end = TInstant::Now();
            if (end - start >= maxDelay) {
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                return;
            }
        }

        if (Num >= 2) {
            MaxDivisor = Num;
        }

        if (OriginNum == 1) {
            MaxDivisor = 1;
        }

        Send(WriteActorId, std::make_unique<TEvents::TEvWriteValueRequest>(MaxDivisor));
        Send(ReadActorId, std::make_unique<TEvents::TEvDone>());
        PassAway();
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

    STRICT_STFUNC(StateFunc) {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    }

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
