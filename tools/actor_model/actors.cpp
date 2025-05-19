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

// TODO: напишите реализацию TReadActor

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

// TODO: напишите реализацию TMaximumPrimeDevisorActor

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

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t OriginalNumber;
    NActors::TActorId ReadActorId;
    NActors::TActorId WriteActorId;

    int64_t N; 
    int64_t D = 2; 
    int64_t MaxPrimeFactor = 1;
    bool CalculationDone = false;

public:
    TMaximumPrimeDevisorActor(int64_t number, NActors::TActorId readActorId, NActors::TActorId writeActorId)
        : OriginalNumber(number)
        , ReadActorId(readActorId)
        , WriteActorId(writeActorId)
        , N(number)
    {
        if (OriginalNumber <= 0) {
            MaxPrimeFactor = 0;
            N = 0; 
        } else if (OriginalNumber == 1) {
            MaxPrimeFactor = 1;
            N = 1; 
        }
    }

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateWork);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateWork, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
        if (CalculationDone) return;

        if (N <= 0) { 
            SendResultsAndFinish(0);
            return;
        }
        if (N == 1) { 
            SendResultsAndFinish(MaxPrimeFactor);
            return;
        }

        TInstant startTime = TInstant::Now();

        while (N > 1) { 
            if (TInstant::Now() - startTime > TDuration::MilliSeconds(10)) {
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                return; 
            }

            if (D * D > N) { 
                MaxPrimeFactor = N; 
                N = 1;              
                break;              
            }

            if (N % D == 0) {    
                MaxPrimeFactor = D; 
                while (N % D == 0)
                    N /= D;
            }
            
            if (D == 2) D = 3;
            else D += 2;
        }
        SendResultsAndFinish(MaxPrimeFactor);
    }

    void SendResultsAndFinish(int64_t result) {
        if (CalculationDone) return; 

        Send(WriteActorId, MakeHolder<TEvents::TEvWriteValueRequest>(result));
        Send(ReadActorId, MakeHolder<TEvents::TEvDone>());
        CalculationDone = true;
        PassAway();
    }
};

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    std::istream& Strm;
    NActors::TActorId WriteActorRecipientId;
    int PendingDevisorActors = 0;
    bool ReadingDone = false;

public:
    TReadActor(std::istream& strm, NActors::TActorId writeActorRecipientId)
        : Strm(strm)
        , WriteActorRecipientId(writeActorRecipientId)
    {}

    void Bootstrap() {
        Become(&TReadActor::StateRead);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateRead, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        hFunc(TEvents::TEvDone, HandleDone);
    });

    void HandleWakeup() {
        int64_t value;
        if (Strm >> value) {
            Register(new TMaximumPrimeDevisorActor(value, SelfId(), WriteActorRecipientId));
            PendingDevisorActors++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            ReadingDone = true;
            CheckCompletion();
        }
    }

    void HandleDone(TEvents::TEvDone::TPtr&) {
        PendingDevisorActors--;
        CheckCompletion();
    }

    void CheckCompletion() {
        if (ReadingDone && PendingDevisorActors == 0) {
            Send(WriteActorRecipientId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            PassAway();
        }
    }
};

class TWriteActor : public NActors::TActor<TWriteActor> {
    int64_t Sum = 0;

public:
    TWriteActor() : NActors::TActor<TWriteActor>(&TWriteActor::StateWork) {}

    STRICT_STFUNC(StateWork, {
        hFunc(TEvents::TEvWriteValueRequest, HandleWriteValueRequest);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    });

    void HandleWriteValueRequest(TEvents::TEvWriteValueRequest::TPtr& ev) {
        auto* msg = ev->Get();
        Sum += msg->Value;
    }

    void HandlePoisonPill() {
        std::cout << Sum << std::endl;
        GetProgramShouldContinue()->ShouldStop(0); 
        PassAway();
    }
};

THolder<NActors::IActor> CreateReadActor(std::istream& strm, NActors::TActorId writeActorRecipientId) {
    return MakeHolder<TReadActor>(strm, writeActorRecipientId);
}

THolder<NActors::IActor> CreateWriteActor() {
    return MakeHolder<TWriteActor>();
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

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
