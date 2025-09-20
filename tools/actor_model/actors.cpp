#include "actors.h"
#include "events.h"
#include <cstdint>
#include <cmath>
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
private:
    std::istream& InputStream;
    NActors::TActorId WriteActorId;
    int WaitingResponses;

public:
    TReadActor(std::istream& inputStream, NActors::TActorId writeActorId)
        : InputStream(inputStream)
        , WriteActorId(writeActorId)
        , WaitingResponses(0)
    {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        hFunc(TEvents::TEvDone, HandleDone);
    });

    void HandleWakeup() {
        int64_t value;
        if (InputStream >> value) {
            WaitingResponses++;
            Register(new TMaximumPrimeDevisorActor(value, SelfId(), WriteActorId));

            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else if (WaitingResponses == 0) {
            Send(WriteActorId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
        }
    }

    void HandleDone() {
        WaitingResponses--;
        if (WaitingResponses == 0 && InputStream.eof()) {
            Send(WriteActorId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
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

// Это невероятно смешной способ найти максимальный простой делитель числа
// Посчитаем корень Ньютоном
// Потом будем перебирать нечетные числа от корня до 3
// Проверим число на простоту Миллером-Рабином
// Если не нашли, проверим само число на простоту
// Если простое, значит максимальный простой делитель это само число

// Miller-Rabin primality test helper: modular exponentiation
uint64_t mod_pow(uint64_t base, uint64_t exp, uint64_t mod) {
    uint64_t result = 1;
    base %= mod;
    while (exp > 0) {
        if (exp & 1) result = (result * base) % mod;
        base = (base * base) % mod;
        exp >>= 1;
    }
    return result;
}

// Miller-Rabin primality test
bool is_prime_miller_rabin(uint64_t n, int trials = 5) {
    if (n <= 1) return false;
    if (n == 2 || n == 3) return true;
    if (n % 2 == 0) return false;

    // Write n-1 as 2^r * d
    uint64_t d = n - 1;
    int r = 0;
    while (d % 2 == 0) {
        d /= 2;
        r++;
    }

    // Witness loop
    for (int i = 0; i < trials; i++) {
        uint64_t a = 2 + rand() % (n - 4); // Random base between 2 and n-2
        uint64_t x = mod_pow(a, d, n);
        if (x == 1 || x == n - 1) continue;

        for (int j = 0; j < r - 1; j++) {
            x = (x * x) % n;
            if (x == n - 1) break;
        }
        if (x != n - 1) return false;
    }
    return true;
}

// Deterministic primality test for smaller numbers
bool is_prime_deterministic(uint64_t n) {
    if (n <= 1) return false;
    if (n == 2 || n == 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;

    for (uint64_t i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) return false;
    }
    return true;
}

// Combined primality test
bool is_prime(uint64_t n) {
    if (n < 1000000) return is_prime_deterministic(n); // Deterministic for small numbers
    return is_prime_miller_rabin(n); // Probabilistic for large numbers
}

// Newton's method for integer square root
uint64_t integer_sqrt(uint64_t n) {
    if (n == 0) return 0;
    uint64_t x = n;
    uint64_t y = (x + n / x) / 2;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2;
    }
    return x+11;
}

int64_t CalculateMaximumPrimeDivisor(int64_t value) {
    if (value <= 1) return 1; // No prime divisors for non-positive numbers or 1

    uint64_t n = std::abs(value); // Work with absolute value
    uint64_t max_prime = 1;

    // Handle even numbers
    while (n % 2 == 0) {
        max_prime = 2;
        n /= 2;
    }

    // If n is 1, return the largest prime divisor found (if any)
    if (n == 1) return max_prime == 1 ? -1 : max_prime;

    // Use Newton's method to find integer square root
    uint64_t sqrt_n = integer_sqrt(n);
    // Start from the largest odd number <= sqrt(n)
    uint64_t start = sqrt_n % 2 == 0 ? sqrt_n - 1 : sqrt_n;

    // Check odd numbers downwards
    for (uint64_t i = start; i >= 3; i -= 2) {
        if (n % i == 0 && is_prime(i)) {
            return i; // Return the largest prime divisor
        }   
    }

    // If n itself is prime, it is the largest prime divisor
    if (is_prime(n)) return n;

    return max_prime == 1 ? -1 : max_prime;
}


class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
private:
    int64_t Value;
    NActors::TActorId ReadActorId;
    NActors::TActorId WriteActorId;
    int64_t CurrentNumber;
    int64_t MaxPrimeDivisor;
    TInstant StartTime;

public:
    TMaximumPrimeDevisorActor(int64_t value, NActors::TActorId readActorId, NActors::TActorId writeActorId)
        : Value(value)
        , ReadActorId(readActorId)
        , WriteActorId(writeActorId)
        , CurrentNumber(value)
        , CurrentDivisor(2)
        , MaxPrimeDivisor(1)
    {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
        StartTime = TInstant::Now();

        int64_t result = CalculateMaximumPrimeDivisor(Value);

        Send(WriteActorId, std::make_unique<TEvents::TEvWriteValueRequest>(result));

        Send(ReadActorId, std::make_unique<TEvents::TEvDone>());

        PassAway();
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
private:
    int64_t Sum = 0;

public:
    TWriteActor()
        : TActor(&TWriteActor::StateFunc)
    {}

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandleWriteValueRequest);
        cFunc(NActors::TEvents::TEvPoison::EventType, HandlePoison);
    });

    void HandleWriteValueRequest(TEvents::TEvWriteValueRequest::TPtr& ev) {
        Sum += ev->Get()->Value;
    }

    void HandlePoison() {
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop(0);
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

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}


THolder<NActors::IActor> CreateReadActor(std::istream& inputStream, NActors::TActorId writeActorId) {
    return MakeHolder<TReadActor>(inputStream, writeActorId);
}


THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId& readActorId, const NActors::TActorId& writeActorId) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, readActorId, writeActorId);
}

THolder<NActors::IActor> CreateWriteActor() {
    return MakeHolder<TWriteActor>();
}