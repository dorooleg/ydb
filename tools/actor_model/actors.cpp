#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();


/*
Вам нужно написать реализацию TReadActor, TMaximumPrimeDevisorActor, TWriteActor
*/

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

TReadActor
    Bootstrap:
        send(self, NActors::TEvents::TEvWakeup)

    NActors::TEvents::TEvWakeup:
        if read(strm) -> value:
            register(TMaximumPrimeDevisorActor(value, self, receipment))
            send(self, NActors::TEvents::TEvWakeup)
        else:
            ...

    TEvents::TEvDone:
        if Finish:
            send(receipment, NActors::TEvents::TEvPoisonPill)
        else:
            ...
*/

// реализацию TReadActor
class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    int64_t NPendingActors = 0;
    bool FlagDone = false;
    const NActors::TActorId WActor;

public:
    TReadActor(const NActors::TActorId wActor)
        : WActor(wActor) {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    void HandleWakeUp() {
        int64_t inputV;
        if (std::cin >> inputV) {
            Register(CreateMaximumPrimeDevisorActor(inputV, SelfId(), WActor).Release());
            NPendingActors+=1;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            FlagDone = true;
            if (NPendingActors == 0) {
                SendPoisonPill();
            }
        }
    }

    void HandleDone() {
        NPendingActors-=1;
        if (FlagDone && NPendingActors == 0) {
            SendPoisonPill();
        }
    }

    void SendPoisonPill() {
        Send(WActor, std::make_unique<NActors::TEvents::TEvPoisonPill>());
        PassAway();
    }

    STFUNC(StateFunc) {
        switch (ev->GetTypeRewrite()) {
            sFunc(NActors::TEvents::TEvWakeup, HandleWakeUp);
            sFunc(TEvents::TEvDone, HandleDone);
            default:
                break;
        }
    }
};
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

TMaximumPrimeDevisorActor
    Bootstrap:
        send(self, NActors::TEvents::TEvWakeup)

    NActors::TEvents::TEvWakeup:
        calculate
        if > 10 ms:
            Send(SelfId(), NActors::TEvents::TEvWakeup)
        else:
            Send(WriteActor, TEvents::TEvWriteValueRequest)
            Send(ReadActor, TEvents::TEvDone)
            PassAway()
*/

// реализацию TMaximumPrimeDevisorActor
class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t Value;
    const NActors::TActorId RActor;
    const NActors::TActorId WActor;
    int64_t largestPrimeDevisor = 1;// In case Value is 1, the largest prime divisor of 1 is itself, so largestPrimeDevisor needs to be initialized to 1. 
    int64_t currDevisor = 2; // 2 is the smallest prime number.

public:
    TMaximumPrimeDevisorActor(int64_t inputV, const NActors::TActorId rActor, const NActors::TActorId wActor)
        : Value(inputV), RActor(rActor), WActor(wActor) {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    void HandleWakeUp() {
        if(Value < 1){
            Send(WActor, std::make_unique<TEvents::TEvWriteValueRequest>(0));
            Send(RActor, std::make_unique<TEvents::TEvDone>());
            PassAway();
            return;
        }
        auto startTime = std::chrono::steady_clock::now();

        while (currDevisor * currDevisor <= Value) {
            if (!(Value % currDevisor)) {
                largestPrimeDevisor = currDevisor;
                Value /= currDevisor;
            } else {
                currDevisor+=1;
            }
            auto runTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();
            if (runTime > 10) {
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                return;
            }
        }

        if (Value > 1) {
            largestPrimeDevisor = Value;
        }

        Send(WActor, std::make_unique<TEvents::TEvWriteValueRequest>(largestPrimeDevisor));
        Send(RActor, std::make_unique<TEvents::TEvDone>());
        PassAway();
    }

    STFUNC(StateFunc) {
        switch (ev->GetTypeRewrite()) {
            sFunc(NActors::TEvents::TEvWakeup, HandleWakeUp);
            default:
                break;
        }
    }
};
/*
Требования к TWriteActor:
1. Рекомендуется отнаследовать этот актор от NActors::TActor
2. Этот актор получает два типа сообщений NActors::TEvents::TEvPoisonPill::EventType и TEvents::TEvWriteValueRequest
2. В случае TEvents::TEvWriteValueRequest он принимает результат посчитанный в TMaximumPrimeDevisorActor и прибавляет его к локальной сумме
4. В случае NActors::TEvents::TEvPoisonPill::EventType актор выводит в Cout посчитанную локальнкую сумму, проставляет ShouldStop и завершает свое выполнение через PassAway

TWriteActor
    TEvents::TEvWriteValueRequest ev:
        Sum += ev->Value

    NActors::TEvents::TEvPoisonPill::EventType:
        Cout << Sum << Endl;
        ShouldStop()
        PassAway()
*/

// реализацию TWriteActor
class TWriteActor : public NActors::TActor<TWriteActor> {
    int64_t Sum = 0;

public:
    TWriteActor() : TActor(&TWriteActor::StateFunc) {}

    void HandleWriteValueRequest(TEvents::TEvWriteValueRequest::TPtr& ev) {
        Sum += ev->Get()->Value;
    }

    void HandlePoisonPill() {
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }

    STFUNC(StateFunc) {
        switch (ev->GetTypeRewrite()) {
            hFunc(TEvents::TEvWriteValueRequest, HandleWriteValueRequest);
            sFunc(NActors::TEvents::TEvPoisonPill, HandlePoisonPill);
            default:
               break;
        }
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

THolder<NActors::IActor> CreateTReadActor(const NActors::TActorId wActor) {
    return MakeHolder<TReadActor>(wActor);
}

THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t inputV, const NActors::TActorId rActor, const NActors::TActorId wActor) {
    return MakeHolder<TMaximumPrimeDevisorActor>(inputV, rActor, wActor);
}

THolder<NActors::IActor> CreateTWriteActor() {
    return MakeHolder<TWriteActor>();
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
