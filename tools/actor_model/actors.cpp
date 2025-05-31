#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TMaxPrimeDivisorActor;

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
class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    const NActors::TActorId m_WriterActorId;
    size_t m_ActiveWorkersCount = 0;
    bool m_InputFinished = false;

public:
    TReadActor(NActors::TActorId writerActorId)
        : m_WriterActorId(writerActorId)
    {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

private:
    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        cFunc(TEvents::EvDone, HandleWorkerDone);
    });

    void HandleWakeup() {
        int64_t inputNumber;
        if (std::cin >> inputNumber) {
            m_ActiveWorkersCount++;
            Register(CreateMaxPrimeDivisorActor(inputNumber, SelfId(), m_WriterActorId).Release());
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
        } else {
            m_InputFinished = true;
            if (m_ActiveWorkersCount == 0) {
                Shutdown();
            }
        }
    }

    void HandleWorkerDone() {
        m_ActiveWorkersCount--;
        if (m_InputFinished && m_ActiveWorkersCount == 0) {
            Shutdown();
        }
    }

    void Shutdown() {
        Send(m_WriterActorId, new NActors::TEvents::TEvPoisonPill());
        PassAway();
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

int64_t FindLargestPrimeDivisor(int64_t number) {
    if (number <= 1) {
        return 1;
    }

    int64_t largestPrime = -1;

    // Handle even numbers
    while (number % 2 == 0) {
        largestPrime = 2;
        number /= 2;
    }

    // Check odd divisors up to sqrt(n)
    for (int64_t divisor = 3; divisor * divisor <= number; divisor += 2) {
        while (number % divisor == 0) {
            largestPrime = divisor;
            number /= divisor;
        }
    }

    // If remaining number is prime
    if (number > 1) {
        largestPrime = number;
    }

    return largestPrime != -1 ? largestPrime : 1;
}

// TODO: напишите реализацию TMaximumPrimeDevisorActor
class TMaxPrimeDivisorActor : public NActors::TActorBootstrapped<TMaxPrimeDivisorActor> {
    const int64_t m_InputNumber;
    const NActors::TActorId m_ReaderActorId;
    const NActors::TActorId m_WriterActorId;

public:
    TMaxPrimeDivisorActor(int64_t number, 
                         NActors::TActorId readerActorId, 
                         NActors::TActorId writerActorId)
        : m_InputNumber(number)
        , m_ReaderActorId(readerActorId)
        , m_WriterActorId(writerActorId)
    {}

    void Bootstrap() {
        ProcessAndRespond();
    }

private:
    void ProcessAndRespond() {
        int64_t result = FindLargestPrimeDivisor(m_InputNumber);

        Send(m_WriterActorId, new TEvents::TEvWriteValueRequest(result));
        Send(m_ReaderActorId, new TEvents::TEvDone());
        PassAway();
    }
};

THolder<NActors::IActor> CreateMaxPrimeDivisorActor(int64_t value, 
                                                   NActors::TActorId readerActorId, 
                                                   NActors::TActorId writerActorId) {
    return MakeHolder<TMaxPrimeDivisorActor>(value, readerActorId, writerActorId);
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
class TWriteActor : public NActors::TActor<TWriteActor> {
    int64_t m_ResultSum = 0;

public:
    TWriteActor() : TActor(&TWriteActor::StateFunc) {}

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandleResult);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandleShutdown);
    });

    void HandleResult(TEvents::TEvWriteValueRequest::TPtr& event) {
        m_ResultSum += event->Get()->Value;
    }

    void HandleShutdown() {
        std::cout << m_ResultSum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }
};

class TSystemMonitorActor : public NActors::TActorBootstrapped<TSystemMonitorActor> {
    TDuration m_MaxAllowedLatency;
    TInstant m_LastPingTime;

public:
    TSystemMonitorActor(const TDuration& maxLatency)
        : m_MaxAllowedLatency(maxLatency)
    {}

    void Bootstrap() {
        m_LastPingTime = TInstant::Now();
        Become(&TSystemMonitorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandlePing);
    });

    void HandlePing() {
        auto currentTime = TInstant::Now();
        TDuration actualLatency = currentTime - m_LastPingTime;
        Y_VERIFY(actualLatency <= m_MaxAllowedLatency, "System latency exceeded allowed maximum");
        m_LastPingTime = currentTime;
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }
};

THolder<NActors::IActor> CreateSystemMonitorActor(const TDuration& latency) {
    return MakeHolder<TSystemMonitorActor>(latency);
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}

THolder<NActors::IActor> CreateReadActor(NActors::TActorId writerActorId) {
    return MakeHolder<TReadActor>(writerActorId);
}

THolder<NActors::IActor> CreateWriteActor() {
    return MakeHolder<TWriteActor>();
}
