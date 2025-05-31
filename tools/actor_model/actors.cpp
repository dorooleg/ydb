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


class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    NActors::TActorId WriteTarget;
    int64_t Amount;
    int64_t Answers;
    bool IsRead;
    std::istream& STRM;

public:
    TReadActor(NActors::TActorId target, std::istream& strm)
        : WriteTarget(target)
        , Amount(0)
        , Answers(0)
        , IsRead(true)
        , STRM(strm)
    {}

    void Bootstrap(){
        Become(&TThis::StateRead);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    STFUNC(StateRead){
        switch(ev->GetTypeRewrite()){
            sFunc(NActors::TEvents::TEvWakeup, Read);
            sFunc(TEvents::TEvDone, Done);
        }
    }

    void Done(){
        Answers++;
        if (Answers == Amount && !IsRead){
            Send(WriteTarget, new NActors::TEvents::TEvPoisonPill());
            PassAway();
        }
    }
    void Read(){
        int64_t value;
        if (STRM >> value){
            Register(CreateMaximumPrimeDevisorActor(SelfId(), WriteTarget, value));
            Amount++;
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
        }else{
            IsRead = false;
            if (!Amount){
                Send(WriteTarget, new NActors::TEvents::TEvPoisonPill());
                PassAway();
            }
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

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    NActors::TActorId ReadTarget;
    NActors::TActorId WriteTarget;
    int64_t Number;
    TDuration TimeDur; // период работы
    int64_t CurrentNumber;
    int64_t MaxPrime;

public:
    TMaximumPrimeDevisorActor(NActors::TActorId readTarget, NActors::TActorId writeTarget, int64_t number)
        : ReadTarget(readTarget)
        , WriteTarget(writeTarget)
        , Number(number)
        , TimeDur(TDuration::MilliSeconds(10))
        , CurrentNumber(2)
        , MaxPrime(1)
    {}
    
    STFUNC(StateCalc){
        switch(ev->GetTypeRewrite()){
            sFunc(NActors::TEvents::TEvWakeup, Handler);
        }
    }
    
    void Handler(){
        if (Number < 0) Number = -1 * Number;
        auto end = TInstant::Now() + TimeDur;

        while(TInstant::Now() < end && CurrentNumber <= Number){
            if (Number % CurrentNumber == 0 && IsPrime(CurrentNumber)) MaxPrime = CurrentNumber;
            // Cout << MaxPrime << " " << IsPrime(MaxPrime)  Endl;
            CurrentNumber++;
        }

        if (CurrentNumber > Number){
            // Cout << MaxPrime << " " << IsPrime(MaxPrime) << Endl;
            Send(WriteTarget, new TEvents::TEvWriteValueRequest(MaxPrime));
            Send(ReadTarget, new TEvents::TEvDone());
            PassAway();
        }else{
            // Cout << "Stop and continue calc\n";
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
        }
    }

    bool IsPrime(int64_t x){
        if (x < 2) return false;
        for (int64_t i = 2; i < x; i++){
            if (x % i == 0) return false;
        }
        return true;
    }

    void Bootstrap(){
        Become(&TThis::StateCalc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
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


class TWriteActor : public NActors::TActor<TWriteActor> {
    int64_t Sum;

public:
    TWriteActor()
        : TActor(&TThis::StateWrite)
        , Sum(0)
    {}
    
    STFUNC(StateWrite){
        switch(ev->GetTypeRewrite()){
            hFunc(TEvents::TEvWriteValueRequest, SumFunc);
            cFunc(NActors::TEvents::TEvPoisonPill::EventType, Print);
        }
    }
    void SumFunc(TEvents::TEvWriteValueRequest::TPtr &ev){
        // Cout << "sum: ";
        // Cout << Sum << Endl;
        Sum += ev->Get()->Value;
    }
    void Print(){
        Cout << Sum << Endl;
        ShouldContinue->ShouldStop();
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

NActors::IActor * CreateWriteActor(){
    return new TWriteActor();
}

NActors::IActor * CreateReadActor(NActors::TActorId writer, std::istream& strm){
    return new TReadActor(writer, strm);
}

NActors::IActor * CreateMaximumPrimeDevisorActor(NActors::TActorId reader, NActors::TActorId writer, int64_t value){
    return new TMaximumPrimeDevisorActor(reader, writer, value);
}

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency) {
    return MakeHolder<TSelfPingActor>(latency);
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
