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

class TReadActor: public NActors::TActorBootstrapped<TReadActor> {
    private:

        NActors::TActorId writer_id;
        int actors_counter = 0;
        bool finish= false;

        void Handel(){
            // std::cout << "handel" << std::endl;
            int64_t input;
            while (std::cin >> input){
                Register(CreateTMaximumPrimeDevisorActor(SelfId(), writer_id, input).Release());
                actors_counter ++;
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
            }

            finish = true;
            // std::cout << "handel_finish" << std::endl;
        }

        void HandelDone(){
            // std::cout << "handelDONE" << std::endl;
            actors_counter --;
            if (actors_counter == 0 && finish)
            {
                Send(writer_id, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            }
            
        }


    public:

        TReadActor(NActors::TActorId writer_id)
        : writer_id(writer_id){}
    
        void Bootstrap() {
            // std::cout << "test" << std::endl;
            Become(&TReadActor::StateFunc);
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
            
        }

        STRICT_STFUNC(StateFunc, {
            cFunc(NActors::TEvents::TEvWakeup::EventType, Handel);
            cFunc(TEvents::TEvDone::EventType, HandelDone);
        });

};

    THolder<NActors::IActor> CreateTReadActor(NActors::TActorId writer_id) {
        return MakeHolder<TReadActor>(writer_id);
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

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor>
{
private:
    NActors::TActorId reader_id;
    NActors::TActorId writer_id;
    int64_t value;
    bool time_out = false;
    TInstant time_start;


public:
    TMaximumPrimeDevisorActor(int64_t value, NActors::TActorId reader_id, NActors::TActorId writer_id)
    : value(value)
    , reader_id(reader_id)
    , writer_id(writer_id){}

    void Bootstrap() {
    Become(&TMaximumPrimeDevisorActor::StateFunc);
    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, Handel);
    });

    void Handel(){
        // std::cout << "handel DIVizer start" << std::endl;
        time_start = TInstant::Now();
        int64_t div = CalcMaxPrimeDiv(value);
        // std::cout << "time out "<< std::endl;
        // std::cout << time_out << std::endl;
        if (time_out)
        {   
            // std::cout << "handel DIVizer time out true" << std::endl;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        }else{
            // std::cout << "handel DIVizer finish" << std::endl;
            Send(writer_id, std::make_unique<TEvents::TEvWriteValueRequest>(div));
            Send(reader_id, std::make_unique<TEvents::TEvDone>());
            PassAway();
        }
        

    }

    int64_t CalcMaxPrimeDiv(int64_t value){
        std::cout << "calc start" << std::endl;
        std::cout << value << std::endl;
        int64_t result = 0;
        if (value == 1 || value == 0){
            // std::cout << "calc return 1" << std::endl;
            return value;
        }
        while (value % 2 == 0)
        {
            result = 2;
            value /= 2;
            auto time_now = TInstant::Now();
            TDuration time_m = time_now - time_start;
            if (time_m > TDuration::MilliSeconds(10)){
                time_out = true;
                return result;
            }
        }

        for (int i = 3; i <= sqrt(value); i += 2) {
                while (value % i == 0) {
                    result = i;
                    value /= i;
                    auto time_now = TInstant::Now();
                    TDuration time_m = time_now - time_start;
                    if (time_m > TDuration::MilliSeconds(10)) {
                        time_out = true;
                        return result;
                    }
                }
            }
        if (value > 1) {
            result = value;
        }

        return result;
        

    }

};

    THolder<NActors::IActor> CreateTMaximumPrimeDevisorActor(NActors::TActorId read_id, NActors::TActorId write_id, int64_t value) {
       return MakeHolder<TMaximumPrimeDevisorActor>(value, read_id, write_id);
}
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

// TODO: напишите реализацию TWriteActor

class TWriteActor : public NActors::TActor<TWriteActor>{
    private:
        int64_t res = 0;

    public:
    TWriteActor():TActor(&TWriteActor::StateFunc) {}

    STRICT_STFUNC(StateFunc, {
         hFunc(TEvents::TEvWriteValueRequest, GetVal);
         cFunc(NActors::TEvents::TEvPoisonPill::EventType, Output);
    });

    void GetVal(TEvents::TEvWriteValueRequest::TPtr& ev){
        res += ev->Get()->value;
    }

    void Output(){
        std::cout << res << std::endl;
        GetProgramShouldContinue()->ShouldStop();
        PassAway();
    }


};

    THolder<NActors::IActor> CreateTWriteActor() {
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
