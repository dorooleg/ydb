#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <stdint.h>
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
    NActors::TActorId Writer_id;
    int Counter;
public:
    TReadActor(const NActors::TActorId writer_id)
        : Writer_id(writer_id), Counter(0)
    {}
    

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        cFunc(TEvents::TEvDone::EventType, HandleEventDone);
    });
    
    void HandleWakeup() {
        int64_t n;
        if (std::cin >> n)
        {
            Counter++;
            Register(CreateTMaximumPrimeDevisorActor(n, Writer_id, SelfId()).Release());
            Send(SelfId(),  std::make_unique<NActors::TEvents::TEvWakeup>());
        }
    }

    void HandleEventDone(){
        Counter--;
        if(Counter == 0){
            Send(Writer_id, std::make_unique<NActors::TEvents::TEvPoisonPill>());
        }
    }
};




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

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t Value;
    NActors::TActorId Writer_id;
    NActors::TActorId Reader_id;
    TInstant StartTime;
    int64_t Cur;
    TDuration Latency;
    int64_t Res;
public:
    TMaximumPrimeDevisorActor(const int64_t& value, const NActors::TActorId& writer_id,const NActors::TActorId& reader_id)
        : Value(value), Writer_id(writer_id), Reader_id(reader_id), Cur(1), Res(0) 
    {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Latency = TDuration::MilliSeconds(10);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }


    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });
    

    void HandleWakeup() {
        if (Res == 0){
            Calculate();
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
            return;
        }
        else{
            Send(Writer_id, std::make_unique<TEvents::TEvWriteValueRequest>(Res));
            Send(Reader_id, std::make_unique<TEvents::TEvDone>());
            PassAway();
        }
    }


    int64_t Calculate() {
        int64_t maxPrime = 1;
        StartTime = TInstant::Now();
        for (int64_t i = Cur; i <= Value; i++) {
            if (Value % i == 0) {
                bool isPrime = true;
                for (int64_t j = 2; j <= i / 2; j++) {
                    if(TInstant::Now() - StartTime > Latency){
                        Cur = i;
                        return 0;
                    }
                    if (i % j == 0) {
                        isPrime = false;
                        break;
                    }
                }
                if (isPrime) {
                    maxPrime = i;
                }
            }
        }
        Res = maxPrime;
        return 1;
    }

};


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

class TWriteActor : public NActors::TActor<TWriteActor> {
    int64_t Sum = 0;

public:
    TWriteActor()
    : TActor(&TThis::StateFunc)
    {}

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePill);
        hFunc(TEvents::TEvWriteValueRequest, Handle)
    });

    void Handle(TEvents::TEvWriteValueRequest::TPtr &ev){
        auto *msg = ev->Get();
        Sum = Sum + msg->Value;  
    }

    void HandlePill(){
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }

};





// TODO: напишите реализацию TWriteActor

// class TSelfPingActor : public NActors::TActorBootstrapped<TSelfPingActor> {
//     TDuration Latency;
//     TInstant LastTime;

// public:
//     TSelfPingActor(const TDuration& latency)
//         : Latency(latency)
//     {}

//     void Bootstrap() {
//         LastTime = TInstant::Now();
//         Become(&TSelfPingActor::StateFunc);
//         Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
//     }

//     STRICT_STFUNC(StateFunc, {
//         cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
//     });

//     void HandleWakeup() {
//         auto now = TInstant::Now();
//         TDuration delta = now - LastTime;
//         Y_VERIFY(delta <= Latency, "Latency too big");
//         LastTime = now;
//         Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
//     }
// };

// THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency) {
//     return MakeHolder<TSelfPingActor>(latency);
// }

THolder<NActors::IActor> CreateTReadActor(const NActors::TActorId writer_id) {
    return MakeHolder<TReadActor>(writer_id);
}

THolder<NActors::IActor> CreateTMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId writer_id, const NActors::TActorId reader_id) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value,writer_id,reader_id);
}

THolder<NActors::IActor> CreateTWriteActor() {
    return MakeHolder<TWriteActor>();
}



std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
