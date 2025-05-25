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

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
private:
    int m_divisorCount;
    int m_numbersCount;
    NActors::TActorId m_writeActorId;
    bool m_isFinished;

public:
    explicit TReadActor(NActors::TActorId writeActorId) 
        : m_divisorCount(0)
        , m_numbersCount(0)
        , m_writeActorId(writeActorId)
        , m_isFinished(false) {
    }

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    STFUNC(StateFunc) {
        switch (ev->GetTypeRewrite()) {
            case NActors::TEvents::TEvWakeup::EventType:
                HandleWakeUp();
                break;
            case TEvents::TEvDone::EventType:
                HandleFinish();
                break;
            default:
                break;
        }
    }

private:
    void HandleWakeUp() {
        int64_t inputNumber;
        while (cin >> inputNumber) {
            ++m_numbersCount;
            Register(CreateMaxPrimeDevActor(SelfId(), m_writeActorId, inputNumber).Release());
            ++m_divisorCount;
            Send(SelfId(), make_unique<NActors::TEvents::TEvWakeup>());
        }

        if (m_numbersCount == 0) {
            ++m_divisorCount;
            Register(CreateMaxPrimeDevActor(SelfId(), m_writeActorId, 0).Release());
        }
        m_isFinished = true;
    }

    void HandleFinish() {
        --m_divisorCount;
        if (m_divisorCount == 0 && m_isFinished) {
            Send(m_writeActorId, make_unique<NActors::TEvents::TEvPoisonPill>());
        }
    }
};

THolder<NActors::IActor> CreateReadActor(NActors::TActorId writeActorId) {
    return MakeHolder<TReadActor>(writeActorId);
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

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
private:
    static constexpr double MAX_EXECUTION_TIME_MS = 10.0;

    NActors::TActorId m_readActorId;
    NActors::TActorId m_writeActorId;
    int64_t m_value;
    bool m_hasTimeout;
    chrono::time_point<chrono::high_resolution_clock> m_startTime;

public:
    TMaximumPrimeDevisorActor(NActors::TActorId readActorId, NActors::TActorId writeActorId, int64_t value)
        : m_readActorId(readActorId)
        , m_writeActorId(writeActorId)
        , m_value(value)
        , m_hasTimeout(false) {
    }

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), make_unique<NActors::TEvents::TEvWakeup>());
    }

    STFUNC(StateFunc) {
        switch (ev->GetTypeRewrite()) {
            case NActors::TEvents::TEvWakeup::EventType:
                HandleWakeUp();
                break;
            default:
                break;
        }
    }

private:
    int64_t CalculatePrimeDivisor(int64_t value) {
        if (value == 1 || value == -1 || value == 0) {
            return abs(value);
        }

        int64_t result = -1;
        

        while (value % 2 == 0) {
            result = 2;
            value /= 2;
            
            if (IsTimeoutExceeded()) {
                m_hasTimeout = true;
                return result;
            }
        }

        for (int64_t i = 3; i <= static_cast<int64_t>(sqrt(value)); i += 2) {
            while (value % i == 0) {
                result = i;
                value /= i;
                
                if (IsTimeoutExceeded()) {
                    m_hasTimeout = true;
                    return result;
                }
            }
        }

        if (value > 2) {
            result = value;
        }

        return result;
    }

    bool IsTimeoutExceeded() const {
        auto currentTime = chrono::high_resolution_clock::now();
        chrono::duration<double, milli> elapsed = currentTime - m_startTime;
        return elapsed.count() > MAX_EXECUTION_TIME_MS;
    }

    void HandleWakeUp() {
        m_hasTimeout = false;
        m_startTime = chrono::high_resolution_clock::now();
        
        int64_t maxDivisor = CalculatePrimeDivisor(m_value);
        
        if (m_hasTimeout) {
            Send(SelfId(), make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            Send(m_writeActorId, make_unique<TEvents::TEvWriteValueRequest>(maxDivisor));
            Send(m_readActorId, make_unique<TEvents::TEvDone>());
            PassAway();
        }
    }
};

THolder<NActors::IActor> CreateMaxPrimeDevActor(NActors::TActorIdentity readActorId, NActors::TActorId writeActorId, int64_t value) {
    return MakeHolder<TMaximumPrimeDevisorActor>(readActorId, writeActorId, value);
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

class TWriteActor : public NActors::TActor<TWriteActor> {
private:
    int64_t m_sum;

public:
    TWriteActor() 
        : TActor(&TWriteActor::StateFunc)
        , m_sum(0) {
    }

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandleDivisor);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandleOutput);
    });

private:
    void HandleDivisor(TEvents::TEvWriteValueRequest::TPtr& ev) {
        m_sum += ev->Get()->value;
    }
    
    void HandleOutput() {
        cout << m_sum << endl;
        GetProgramShouldContinue()->ShouldStop();
        PassAway();
    }
};

THolder<NActors::IActor> CreateWriteActor() {
    return MakeHolder<TWriteActor>();
}

class TSelfPingActor : public NActors::TActorBootstrapped<TSelfPingActor> {
private:
    TDuration m_latency;
    TInstant m_lastTime;

public:
    explicit TSelfPingActor(const TDuration& latency)
        : m_latency(latency) {
    }

    void Bootstrap() {
        m_lastTime = TInstant::Now();
        Become(&TSelfPingActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

private:
    void HandleWakeup() {
        auto now = TInstant::Now();
        TDuration delta = now - m_lastTime;
        Y_VERIFY(delta <= m_latency, "Latency too big"); // Проверка на то, что задержка не больше, чем заданная
        m_lastTime = now;
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>()); // Самопинг
    }
};

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency) {
    return MakeHolder<TSelfPingActor>(latency);
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
