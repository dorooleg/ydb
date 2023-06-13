#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

/*
Вам нужно написать реализацию TReadActor, TMaximumPrimeDevisorActor, TWriteActor
Вам нужно написать реализацию TReadActor, TMaximumPrimeDivisorActor, TWriteActor
*/

/*
Требования к TReadActor:
1. Рекомендуется отнаследовать этот актор от NActors::TActorBootstrapped
2. В Boostrap этот актор отправляет себе NActors::TEvents::TEvWakeup
3. После получения этого сообщения считывается новое int64_t значение из strm
4. После этого порождается новый TMaximumPrimeDevisorActor который занимается вычислениями
4. После этого порождается новый TMaximumPrimeDivisorActor который занимается вычислениями
5. Далее актор посылает себе сообщение NActors::TEvents::TEvWakeup чтобы не блокировать поток этим актором
6. Актор дожидается завершения всех TMaximumPrimeDevisorActor через TEvents::TEvDone
7. Когда чтение из файла завершено и получены подтверждения от всех TMaximumPrimeDevisorActor,
6. Актор дожидается завершения всех TMaximumPrimeDivisorActor через TEvents::TEvDone
7. Когда чтение из файла завершено и получены подтверждения от всех TMaximumPrimeDivisorActor,
этот актор отправляет сообщение NActors::TEvents::TEvPoisonPill в TWriteActor
TReadActor
    Bootstrap:
        send(self, NActors::TEvents::TEvWakeup)
    NActors::TEvents::TEvWakeup:
        if read(strm) -> value:
            register(TMaximumPrimeDevisorActor(value, self, receipment))
            register(TMaximumPrimeDivisorActor(value, self, receipment))
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
class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    bool Finish;
    const NActors::TActorId WriteActorId;
    int Count = 0;

public:
    TReadActor(const NActors::TActorId writeActorId)
        : Finish(false), WriteActorId(writeActorId)
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
            Register(CreateMaximumPrimeDivisorActor(value, SelfId(), WriteActorId).Release());
            Count++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            Finish = true;
            if (Count == 0) {
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

THolder<NActors::IActor> CreateReadActor(const NActors::TActorId writeActorId) {
    return MakeHolder<TReadActor>(writeActorId);
}

/*
Требования к TMaximumPrimeDevisorActor:
Требования к TMaximumPrimeDivisorActor:
1. Рекомендуется отнаследовать этот актор от NActors::TActorBootstrapped
2. В конструкторе этот актор принимает:
 - значение для которого нужно вычислить простое число
@@ -54,7 +100,7 @@ TReadActor
6. Далее отправляет ReadActor сообщение TEvents::TEvDone
7. Завершает свою работу
TMaximumPrimeDevisorActor
TMaximumPrimeDivisorActor
    Bootstrap:
        send(self, NActors::TEvents::TEvWakeup)
@@ -68,13 +114,74 @@ TMaximumPrimeDevisorActor
            PassAway()
*/

// TODO: напишите реализацию TMaximumPrimeDevisorActor
class TMaximumPrimeDivisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDivisorActor> {
    const NActors::TActorId ReadActorId;
    const NActors::TActorId WriteActorId;
    int64_t InitialValue;
    int64_t Value;
    int64_t CurrentDivisor = 2;
    int64_t MaxDivisor = 0;

public:
    TMaximumPrimeDivisorActor(const int64_t value, const NActors::TActorId readActorId, const NActors::TActorId writeActorId)
            : ReadActorId(readActorId), WriteActorId(writeActorId), InitialValue(abs(value)), Value(abs(value))
    {}

    void Bootstrap() {
        Become(&TMaximumPrimeDivisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
        auto start = TInstant::Now();
        TInstant end;
        TDuration max_delay = TDuration::MilliSeconds(10);
        for (int64_t i=CurrentDivisor; i <= sqrt(Value); i+=(i==2?1:2)) {
            CurrentDivisor = i;

            while (Value % CurrentDivisor == 0) {
                MaxDivisor = CurrentDivisor;
                Value /= CurrentDivisor;
                end = TInstant::Now();
                if (end - start >= max_delay) {
                    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                    return;
                }
            }
            end = TInstant::Now();
            if (end - start >= max_delay) {
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                return;
            }
        }

        if (Value >= 2) {
            MaxDivisor = Value;
        }

        if (InitialValue == 1) {
            MaxDivisor = 1;
        }

        Send(WriteActorId, std::make_unique<TEvents::TEvWriteValueRequest>(MaxDivisor));
        Send(ReadActorId, std::make_unique<TEvents::TEvDone>());
        PassAway();
    }
};

THolder<NActors::IActor> CreateMaximumPrimeDivisorActor(const int64_t value, const NActors::TActorId readActorId, const NActors::TActorId writeActorId) {
    return MakeHolder<TMaximumPrimeDivisorActor>(value, readActorId, writeActorId);
}

/*
Требования к TWriteActor:
1. Рекомендуется отнаследовать этот актор от NActors::TActor
2. Этот актор получает два типа сообщений NActors::TEvents::TEvPoisonPill::EventType и TEvents::TEvWriteValueRequest
2. В случае TEvents::TEvWriteValueRequest он принимает результат посчитанный в TMaximumPrimeDevisorActor и прибавляет его к локальной сумме
2. В случае TEvents::TEvWriteValueRequest он принимает результат посчитанный в TMaximumPrimeDivisorActor и прибавляет его к локальной сумме
4. В случае NActors::TEvents::TEvPoisonPill::EventType актор выводит в Cout посчитанную локальнкую сумму, проставляет ShouldStop и завершает свое выполнение через PassAway
TWriteActor
@@ -87,7 +194,42 @@ TWriteActor
        PassAway()
*/

// TODO: напишите реализацию TWriteActor
class TWriterActor : public NActors::TActorBootstrapped<TWriterActor> {
    int64_t Sum = 0;

public:
    TWriterActor()
    {}

    void Bootstrap() {
        Become(&TWriterActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandleWriteValueRequest);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    });

    void HandleWriteValueRequest(TEvents::TEvWriteValueRequest::TPtr& event) {
        int64_t val = (*event->Get()).Value;
        Sum += val;
    }

    void HandlePoisonPill() {
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }


};

THolder<NActors::IActor> CreateWriterActor() {
    return MakeHolder<TWriterActor>();
}



class TSelfPingActor : public NActors::TActorBootstrapped<TSelfPingActor> {
    TDuration Latency;
  4 changes: 4 additions & 0 deletions4  
tools/actor_model/actors.h
Marking files as viewed can help keep track of your progress, but will not affect your submitted reviewViewed
Comment on this file
@@ -5,4 +5,8 @@

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);

THolder<NActors::IActor> CreateReadActor(const NActors::TActorId writeActorId);
THolder<NActors::IActor> CreateMaximumPrimeDivisorActor(const int64_t value, const NActors::TActorId readActorId, const NActors::TActorId writeActorId);
THolder<NActors::IActor> CreateWriterActor();

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();
  11 changes: 11 additions & 0 deletions11  
tools/actor_model/events.h
Marking files as viewed can help keep track of your progress, but will not affect your submitted reviewViewed
Comment on this file
@@ -3,4 +3,15 @@

struct TEvents {
    // Вам нужно самостоятельно сюда добавить все необходимые events в NActors::TEvents::ES_PRIVATE
    enum EEv {
        EvDiscoveryResponse = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
        EvDone
    };

    struct TEvDone : NActors::TEventLocal<TEvDone, EvDone> {};

    struct TEvWriteValueRequest : NActors::TEventLocal<TEvWriteValueRequest, EvDiscoveryResponse> {
    	int64_t Value;
    	explicit TEvWriteValueRequest(int64_t value) : Value(value) {}
    };
};
  14 changes: 8 additions & 6 deletions14  
tools/actor_model/main.cpp
Marking files as viewed can help keep track of your progress, but will not affect your submitted reviewViewed
Comment on this file
@@ -21,16 +21,18 @@ int main(int argc, const char* argv[])
    NActors::TActorSystem actorSystem(actorySystemSetup);
    actorSystem.Start();

    actorSystem.Register(CreateSelfPingActor(TDuration::Seconds(1)).Release());
    const NActors::TActorId writeActorId = actorSystem.Register(CreateWriterActor().Release());
    actorSystem.Register(CreateReadActor(writeActorId).Release());

    // Зарегистрируйте Write и Read акторы здесь

    // Раскомментируйте этот код
    // auto shouldContinue = GetProgramShouldContinue();
    // while (shouldContinue->PollState() == TProgramShouldContinue::Continue) {
    //     Sleep(TDuration::MilliSeconds(200));
    // }
    auto shouldContinue = GetProgramShouldContinue();
    while (shouldContinue->PollState() == TProgramShouldContinue::Continue) {
     Sleep(TDuration::MilliSeconds(200));
    }
    actorSystem.Stop();
    actorSystem.Cleanup();
    // return shouldContinue->GetReturnCode();
    return shouldContinue->GetReturnCode();
}
