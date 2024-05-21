#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

using namespace std;

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

class TReadActor : public NActors::TActorBootstrapped<TReadActor>{
private:
    int devis_cnt;
    int nums_cnt;
    NActors::TActorId writing;
    bool finished;

public:
    TReadActor(NActors::TActorId writing): writing(writing), devis_cnt(0), nums_cnt(0), finished(false)
    {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    STFUNC(StateFunc) {
        if (ev->GetTypeRewrite() == NActors::TEvents::TEvWakeup::EventType){
            WakeUp();
        }
        else {
            if (ev->GetTypeRewrite() == TEvents::TEvDone::EventType){
                Finish();
            }
        }
    }

    void WakeUp(){
        int64_t input_number;
        while (cin >> input_number){
            nums_cnt++;
            Register(CreateMaxPrimeDevActor(SelfId(), writing, input_number).Release());
            devis_cnt++;
            Send(SelfId(), make_unique<NActors::TEvents::TEvWakeup>());
        }

        // если чисел нет, то в качестве аргумента передается 0
        if (nums_cnt == 0){
            devis_cnt++;
            Register(CreateMaxPrimeDevActor(SelfId(), writing, 0).Release());
        }
        finished = true;
    }

    // чтение закончилось, подтверждения получены
    void Finish(){
        devis_cnt -= 1;
        if (devis_cnt == 0 && finished) {
            Send(writing, make_unique<NActors::TEvents::TEvPoisonPill>());
        }
    }
};

THolder<NActors::IActor> CreateReadActor(NActors::TActorId writing) {
    return MakeHolder<TReadActor>(writing);
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


class TMaximumPrimeDevisorActor :public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor>{
private:
    NActors::TActorId read_actor;
    NActors::TActorId write_actor;
    int64_t value;
    bool has_timeout;
    chrono::time_point<chrono::high_resolution_clock> time_begin;

public:
    TMaximumPrimeDevisorActor(NActors::TActorId read_actor, NActors::TActorId write_actor, int64_t value)
            : read_actor(read_actor), write_actor(write_actor), value(value)
    {}

    void Bootstrap(){
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), make_unique<NActors::TEvents::TEvWakeup>());
    }

    STFUNC(StateFunc){
        if (ev->GetTypeRewrite() == NActors::TEvents::TEvWakeup::EventType){
            WakeUp();
        }
    }

    int64_t GetPrimeDivisor(int64_t value){
        if (value == 1 || value == -1 || value == 0){
            // единица, конечно, простым числом не является, но в тестах считается за простое
            return abs(value);
        }
        else{
            int64_t result = -1;
            // по принципу разложения на простые множители:
            // если число четное, оно делится на 2 (единственное четное простое число)
            // максимальное количество раз
            while (value % 2 == 0) {
                result = 2;
                value /= 2;
                // превышение времени выполнения
                auto time_now = chrono::high_resolution_clock::now();
                chrono::duration<double, milli> elapsed = time_now - time_begin;
                if (elapsed.count() > 10.0) {
                    has_timeout = true;
                    return result;
                }
            }

            // оставшееся число делится на нечетные числа
            for (int i = 3; i <= sqrt(value); i += 2) {
                while (value % i == 0) {
                    result = i;
                    value /= i;
                    // превышение времени выполнения
                    auto time_now = chrono::high_resolution_clock::now();
                    chrono::duration<double, milli> elapsed = time_now - time_begin;
                    if (elapsed.count() > 10.0) {
                        has_timeout = true;
                        return result;
                    }
                }
            }

            // оставшееся число - простое, если больше 2,
            // то является наибольшим простым делителем самого себя
            if (value > 2) {
                result = value;
            }

            return result;
        }

    }

    void WakeUp(){
        has_timeout = false;
        time_begin = chrono::high_resolution_clock::now();
        int64_t max_divisor = GetPrimeDivisor(value);
        if (has_timeout) {
            // произошло прерывание
            Send(SelfId(), make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            Send(write_actor, make_unique<TEvents::TEvWriteValueRequest>(max_divisor));
            Send(read_actor, make_unique<TEvents::TEvDone>());
            PassAway();
        }
    }
};

THolder<NActors::IActor> CreateMaxPrimeDevActor(NActors::TActorIdentity read_actor, NActors::TActorId write_actor, int64_t value) {
    return MakeHolder<TMaximumPrimeDevisorActor>(read_actor, write_actor, value);
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

class TWriteActor: public NActors::TActor<TWriteActor>{
private:
    int64_t summ;
public:

    TWriteActor() : TActor(&TWriteActor::StateFunc), summ(0) {}

    STRICT_STFUNC(StateFunc, {
        // для получения доступа к данным события (у GetDivisor есть параметр)
        hFunc(TEvents::TEvWriteValueRequest, GetDivisor);
        // без параметра
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, GetOutput);
    });

    // получение переданного делителя
    void GetDivisor(TEvents::TEvWriteValueRequest::TPtr& ev){
        summ += ev->Get()->value;
    }
    // сумма и завершение
    void GetOutput(){
        cout << summ << endl;
        GetProgramShouldContinue()->ShouldStop();
        PassAway();
    }
};

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
