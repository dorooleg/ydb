#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <util/system/hp_timer.h>
#include <util/system/yassert.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

/*
Требования к TReadActor:
1. Рекомендуется отнаследовать этот актор от NActors::TActorBootstrapped
2. В Boostrap этот актор отправляет себе NActors::TEvents::TEvWakeup
3. После получения этого сообщения считывается новое int64_t значение из strm
4. После этого порождается новый TMaximumPrimeDevisorActor который занимается вычислениями
5. Далее актор посылает себе сообщение NActors::TEvents::TEvWakeup чтобы не блокировать поток этим актором
6. Актор дожидается завершения всех TMaximumPrimeDevisorActor через TEvents::TEvDone
7. Когда чтение из файла завершено и получены подтверждения от всех TMaximumPrimeDevisorActor,
этот актор отправляет сообщение NActors::TEvents::TEvPoisonPill в TWriteActor
*/

TReadActor::TReadActor(std::istream& input, const NActors::TActorId& writeActor)
    : Input(input)
    , WriteActor(writeActor)
{}

void TReadActor::Bootstrap() {
    Become(&TThis::StateFunc);
    Send(SelfId(), new NActors::TEvents::TEvWakeup());
}

void TReadActor::Handle(NActors::TEvents::TEvWakeup::TPtr&) {
    int64_t value;
    if (Input >> value) {
        ++InFlight;
        auto actor = CreateMaximumPrimeDevisorActor(value, SelfId(), WriteActor);
        Register(actor.Release());
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    } else {
        FinishedReading = true;
        if (InFlight == 0) {
            Send(WriteActor, new NActors::TEvents::TEvPoisonPill());
            PassAway();
        }
    }
}

void TReadActor::Handle(TEvents::TEvDone::TPtr&) {
    --InFlight;
    if (FinishedReading && InFlight == 0) {
        Send(WriteActor, new NActors::TEvents::TEvPoisonPill());
        PassAway();
    }
}

/*
Требования к TMaximumPrimeDevisorActor:
1. Рекомендуется отнаследовать этот актор от NActors::TActorBootstrapped
2. В конструкторе этот актор принимает:
 - значение для которого нужно вычислить простое число
 - ActorId отправителя (ReadActor)
 - ActorId получателя (WriteActor)
2. В Boostrap этот актор отправляет себе NActors::TEvents::TEvWakeup по вызову которого происходит вызов Handler для вычислений
3. Вычисления нельзя проводить больше 10 миллисекунд
4. По истечении этого времени нужно сохранить текущее состояние вычислений в акторе и отправить себе NActors::TEvents::TEvWakeup
5. Когда результат вычислен он посылается в TWriteActor c использованием сообщения TEvWriteValueRequest
6. Далее отправляет ReadActor сообщение TEvents::TEvDone
7. Завершает свою работу
*/

TMaximumPrimeDevisorActor::TMaximumPrimeDevisorActor(int64_t value, NActors::TActorId sender, NActors::TActorId writeActor)
    : Value(value)
    , CurrentDivisor(2)
    , MaxPrime(1)
    , Sender(sender)
    , WriteActor(writeActor)
{}

void TMaximumPrimeDevisorActor::Bootstrap() {
    Become(&TThis::StateFunc);
    Send(SelfId(), new NActors::TEvents::TEvWakeup());
}

void TMaximumPrimeDevisorActor::Handle(NActors::TEvents::TEvWakeup::TPtr&) {
    auto start = GetCycleCount();

    while (CurrentDivisor * CurrentDivisor <= Value) {
        if (Value % CurrentDivisor == 0) {
            bool isPrime = true;
            for (int64_t i = 2; i * i <= CurrentDivisor; ++i) {
                if (CurrentDivisor % i == 0) {
                    isPrime = false;
                    break;
                }
            }
            if (isPrime && CurrentDivisor > MaxPrime) {
                MaxPrime = CurrentDivisor;
            }

            int64_t otherDiv = Value / CurrentDivisor;
            if (otherDiv != CurrentDivisor) {
                isPrime = true;
                for (int64_t i = 2; i * i <= otherDiv; ++i) {
                    if (otherDiv % i == 0) {
                        isPrime = false;
                        break;
                    }
                }
                if (isPrime && otherDiv > MaxPrime) {
                    MaxPrime = otherDiv;
                }
            }
        }

        ++CurrentDivisor;

        if (GetElapsedMicroSeconds(start) > 10'000) {
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
            return;
        }
    }

    if (MaxPrime == 1 && Value > 1) {
        MaxPrime = Value;
    }

    Send(WriteActor, new TEvents::TEvWriteValueRequest(MaxPrime));
    Send(Sender, new TEvents::TEvDone(0));
    PassAway();
}


/*
Требования к TWriteActor:
1. Рекомендуется отнаследовать этот актор от NActors::TActor
2. Этот актор получает два типа сообщений NActors::TEvents::TEvPoisonPill::EventType и TEvents::TEvWriteValueRequest
2. В случае TEvents::TEvWriteValueRequest он принимает результат посчитанный в TMaximumPrimeDevisorActor и прибавляет его к локальной сумме
4. В случае NActors::TEvents::TEvPoisonPill::EventType актор выводит в Cout посчитанную локальнкую сумму, проставляет ShouldStop и завершает свое выполнение через PassAway
*/


TWriteActor::TWriteActor()
    : TActor(&TWriteActor::Receive)
{}

void TWriteActor::Receive(TAutoPtr<NActors::IEventHandle>& ev) {
    switch (ev->GetTypeRewrite()) {
        case TEvents::EvWriteValueRequest: {
            auto* msg = ev->Get<TEvents::TEvWriteValueRequest>();
            Y_ASSERT(msg);
            Sum += msg->Value;
            break;
        }
        case NActors::TEvents::TEvPoisonPill::EventType: {
            std::cout << Sum << std::endl;
            if (auto stopFlag = GetProgramShouldContinue()) {
                stopFlag->ShouldStop();
            }
            PassAway();
            break;
        }
        default:
            Y_FAIL("unexpected event Type: %u", ev->GetTypeRewrite());
    }
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
        Y_VERIFY(delta <= Latency, "Latency too big");
        LastTime = now;
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }
};

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency) {
    return MakeHolder<TSelfPingActor>(latency);
}

THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t value, NActors::TActorId sender, NActors::TActorId writeActor) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, sender, writeActor);
}

ui64 GetElapsedMicroSeconds(ui64 start) {
    return (GetCycleCount() - start) / (ui64)(NHPTimer::GetCyclesPerSecond() / 1000000);
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
