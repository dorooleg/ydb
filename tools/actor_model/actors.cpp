#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <util/system/datetime.h>

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

// TODO: напишите реализацию TWriteActor

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    IInputStream& Strm;
    const NActors::TActorId WriteActor;
    int InFlight = 0;
    bool Finished = false;
    TString CurrentLine;
    size_t Pos = 0;

public:
    TReadActor(IInputStream& strm, const NActors::TActorId& writeActor)
        : Strm(strm), WriteActor(writeActor) {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    STRICT_STFUNC(StateFunc, {
        hFunc(NActors::TEvents::TEvWakeup, HandleWakeup);
        hFunc(TEvents::TEvDone, HandleDone);
    });

    bool ReadNextNumber(int64_t& value) {
        while (true) {
            if (Pos >= CurrentLine.size()) {
                if (!Strm.ReadLine(CurrentLine)) {
                    return false;
                }
                Pos = 0;
                // Удаляем начальные и конечные пробелы вручную
                while (Pos < CurrentLine.size() && isspace(CurrentLine[Pos])) {
                    Pos++;
                }
                size_t end = CurrentLine.size();
                while (end > Pos && isspace(CurrentLine[end-1])) {
                    end--;
                }
                if (end != CurrentLine.size()) {
                    CurrentLine = CurrentLine.substr(Pos, end - Pos);
                    Pos = 0;
                }
            }

            size_t endPos = Pos;
            while (endPos < CurrentLine.size() && !isspace(CurrentLine[endPos])) {
                endPos++;
            }

            if (endPos > Pos) {
                TString numStr = CurrentLine.substr(Pos, endPos - Pos);
                try {
                    value = FromString<int64_t>(numStr);
                    Pos = endPos + 1;
                    return true;
                } catch (...) {
                    Pos = endPos + 1;
                    continue;
                }
            }
            Pos = endPos + 1;
        }
    }

    void HandleWakeup(const NActors::TEvents::TEvWakeup::TPtr&) {
        int64_t value;
        if (ReadNextNumber(value)) {
            Register(CreateMaxPrimeDevActor(value, SelfId(), WriteActor).Release());
            InFlight++;
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
        } else {
            Finished = true;
            if (InFlight == 0) {
                Send(WriteActor, new NActors::TEvents::TEvPoisonPill());
                PassAway();
            }
        }
    }

    void HandleDone(const TEvents::TEvDone::TPtr&) {
        InFlight--;
        if (Finished && InFlight == 0) {
            Send(WriteActor, new NActors::TEvents::TEvPoisonPill());
            PassAway();
        }
    }
};

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t Value;
    const NActors::TActorId ReadActor;
    const NActors::TActorId WriteActor;
    int64_t CurrentDivisor = 2;
    int64_t MaxPrime = 1;

public:
    TMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId& readActor, 
                            const NActors::TActorId& writeActor)
        : Value(value), ReadActor(readActor), WriteActor(writeActor) {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    STRICT_STFUNC(StateFunc, {
        hFunc(NActors::TEvents::TEvWakeup, HandleWakeup);
    });

    void HandleWakeup(const NActors::TEvents::TEvWakeup::TPtr&) {
        TInstant start = TInstant::Now();
        int64_t v = Value;
        
        while (v > 1 && (TInstant::Now() - start).MilliSeconds() <= 10) {
            if (v % CurrentDivisor == 0) {
                MaxPrime = CurrentDivisor;
                while (v % CurrentDivisor == 0) {
                    v /= CurrentDivisor;
                }
            }
            CurrentDivisor++;
        }

        Value = v;
        if (v == 1) {
            Send(WriteActor, new TEvents::TEvWriteValueRequest(MaxPrime));
            Send(ReadActor, new TEvents::TEvDone());
            PassAway();
        } else {
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
        }
    }
};

class TWriteActor : public NActors::TActor<TWriteActor> {
    int64_t Sum = 0;

public:
    TWriteActor()
        : TActor(&TWriteActor::StateFunc) {}

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandleWriteValue);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    });

    void HandleWriteValue(const TEvents::TEvWriteValueRequest::TPtr& ev) {
        Sum += ev->Get()->Value;
    }

    void HandlePoisonPill() {
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }
};

THolder<NActors::IActor> CreateReadActor(IInputStream& strm, const NActors::TActorId& writeActor) {
    return MakeHolder<TReadActor>(strm, writeActor);
}

THolder<NActors::IActor> CreateMaxPrimeDevActor(int64_t value, const NActors::TActorId& readActor, 
                                              const NActors::TActorId& writeActor) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, readActor, writeActor);
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
