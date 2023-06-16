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
*/

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    const NActors::TActorId writer;	
    int count;
    bool is_end;
    bool is_start;

public:
    TReadActor(const NActors::TActorId writer)
        : writer(writer), count(0), is_end(false), is_start(true)
    {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        cFunc(TEvents::TEvDone::EventType, HandleDone);
    });

    void HandleWakeup() {
        int64_t value;
        if (std::cin >> value) {
            is_start = false;	
            Register(CreateTMaximumPrimeDivisorActor(SelfId(), writer, value).Release());
            ++count;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            is_end = true;
            if (is_start) {
            	Register(CreateTMaximumPrimeDivisorActor(SelfId(),  writer, 0).Release());
                ++count;
            	Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
            }
        }
    }
    
    void HandleDone() {
       --count;
       if (count == 0 && is_end) {
            Send(writer, std::make_unique<NActors::TEvents::TEvPoisonPill>());
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
*/

class TMaximumPrimeDivisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDivisorActor> {
    const NActors::TActorId writer;
    const NActors::TActorId reader;
    int64_t value;
    int64_t divisor;
    int64_t largestDivisor;

public:
    TMaximumPrimeDivisorActor(const NActors::TActorId writer, const NActors::TActorId reader, int64_t value)
        : writer(writer), reader(reader), value(value), divisor(2), largestDivisor(1)  {}

    void Bootstrap() {
        Become(&TMaximumPrimeDivisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
        TInstant start = Now();
        while (value > 1 && Now() - start < TDuration::MilliSeconds(10)) {
            while (value % divisor == 0) {
                largestDivisor = divisor;
                value /= divisor;
            }
            ++divisor;
        }
        if (value > 1) {
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            if (divisor - 1 > largestDivisor) {
               largestDivisor = divisor - 1;
            }
            Send(writer, std::make_unique<TEvents::TEvWriteValueRequest>(largestDivisor));
            Send(reader, std::make_unique<TEvents::TEvDone>());
            PassAway();
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

class TWriteActor: public NActors::TActorBootstrapped<TWriteActor>{
    int64_t sum;
public:
    using TBase = NActors::TActor<TWriteActor>;
    TWriteActor()
    : sum(0)
    {}

    void Bootstrap() {
        Become(&TWriteActor::StateFunc);
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandleDone);
        hFunc(TEvents::TEvWriteValueRequest, HandleDivisor);
    });
    
    void HandleDivisor(TEvents::TEvWriteValueRequest::TPtr& message){
        std::cout << sum << std::endl;
        sum += message->Get()->value;
    }

    void HandleDone() {
        std::cout << sum << std::endl;
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

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency) {
    return MakeHolder<TSelfPingActor>(latency);
}

THolder<NActors::IActor> CreateTReadActor(const NActors::TActorId writer) {
    return MakeHolder<TReadActor>(writer);
}

THolder<NActors::IActor> CreateTMaximumPrimeDivisorActor(const NActors::TActorIdentity reader, const NActors::TActorId writer, int64_t value) {
    return MakeHolder<TMaximumPrimeDivisorActor>(reader, writer, value);
}

THolder<NActors::IActor> CreateTWriteActor() {
    return MakeHolder<TWriteActor>();
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}