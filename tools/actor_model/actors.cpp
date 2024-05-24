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
этот актор отправляет сообщение NActors::TEvents::TEvPoisonPill в TWriteActor*/


class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    std::istream& Strm;
    NActors::TActorId Recipient;
    int Counter = 0;
    bool InputEnded;

public:
    TReadActor(std::istream& strm, NActors::TActorId recipient) : Strm(strm), Recipient(recipient), InputEnded() {}

    void Bootstrap() {
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        Become(&TReadActor::StateFunc);
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        cFunc(TEvents::TEvDone::EventType, HandleDone);
    });

    void HandleWakeup() {
        int64_t value;

        while (Strm >> value) {
            Register(CreateMaximumPrimeDevisorActor(value, SelfId(), Recipient).Release());
            Counter++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        }
        InputEnded = true;
    }

    void HandleDone() {
        Counter--;

        if (Counter == 0 && InputEnded) {
            Send(Recipient, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            PassAway();
            // честно говоря мне кажется жестокой система где мы травим и убиваем акторов...
            // неужели не нашлось терминологии погуманнее
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
7. Завершает свою работу */


class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t Value;
    NActors::TActorId ReadActorId;
    NActors::TActorId WriteActorId;
    int64_t CurPrimeDivisor = 1;
    int64_t MaxPrimeDivisor = 0;

public:
    TMaximumPrimeDevisorActor(int64_t value, NActors::TActorId readActorId, NActors::TActorId writeActorId)
    : Value(value), ReadActorId(readActorId), WriteActorId(writeActorId) {}

    void Bootstrap() {
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        Become(&TMaximumPrimeDevisorActor::StateFunc);
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleDivision);
    });

    void HandleDivision() {
        auto now = TInstant::Now();

        while(CurPrimeDivisor <= Value) {

            if (Value == 1) { MaxPrimeDivisor = 1; CurPrimeDivisor++; }

            bool isPrime = true;
            for (int64_t i = 2; i * i <= CurPrimeDivisor; i++) if (CurPrimeDivisor % i == 0) { isPrime = false; break; }
            if (Value % CurPrimeDivisor == 0 && isPrime) MaxPrimeDivisor = CurPrimeDivisor;
            CurPrimeDivisor++;

            if (TInstant::Now() - now > TDuration::MilliSeconds(10)) {
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
            }
        }

        Send(WriteActorId, std::make_unique<TEvents::TEvWriteValueRequest>(MaxPrimeDivisor));
        Send(ReadActorId, std::make_unique<TEvents::TEvDone>());
        PassAway();
    }
};

/*
Требования к TWriteActor:
1. Рекомендуется отнаследовать этот актор от NActors::TActor
2. Этот актор получает два типа сообщений NActors::TEvents::TEvPoisonPill::EventType и
 TEvents::TEvWriteValueRequest
2. В случае TEvents::TEvWriteValueRequest он принимает результат посчитанный в
 TMaximumPrimeDevisorActor и прибавляет его к локальной сумме
4. В случае NActors::TEvents::TEvPoisonPill::EventType актор выводит в Cout
 посчитанную локальнкую сумму, проставляет ShouldStop и завершает свое выполнение через PassAway
*/


class TWriteActor : public NActors::TActor<TWriteActor> {
    int64_t Sum = 0;

public:
    TWriteActor() : TActor(&TWriteActor::StateFunc) {}

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandleRequest);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisoning);
    });

    void HandleRequest(TEvents::TEvWriteValueRequest::TPtr ev) {
        Sum += ev -> Get() -> Value;
    }

    void HandlePoisoning() {
        Cout << Sum << Endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }
};


class TSelfPingActor : public NActors::TActorBootstrapped<TSelfPingActor> {
    TDuration Latency;
    TInstant LastTime;

public:
    TSelfPingActor(const TDuration& latency) : Latency(latency) {}

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

THolder<NActors::IActor> CreateReadActor(std::istream& strm, NActors::TActorId recipient) {
    return MakeHolder<TReadActor>(strm, recipient);
}

THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t value, NActors::TActorId readActorId, NActors::TActorId writeActorId) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, readActorId, writeActorId);
}

THolder<NActors::IActor> CreateWriteActor() {
    return MakeHolder<TWriteActor>();
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
