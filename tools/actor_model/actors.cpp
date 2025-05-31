#include "actors.h"
#include "events.h"

#include <iostream>
#include <vector>

using namespace NActors;

class TReadActor : public TActorBootstrapped<TReadActor> {
    const TActorId MaxActor;

public:
    TReadActor(TActorId maxActor) : MaxActor(maxActor) {}

    void Bootstrap() {
        int x;
        while (std::cin >> x) {
            Send(MaxActor, new TEvInputNumber(x));
        }
        Send(MaxActor, new TEvInputFinished());
        PassAway();
    }
};

IActor* CreateReadActor(TActorId maxActor) {
    return new TReadActor(maxActor);
}


int MaxPrimeDivisor(int n) {
    if (n <= 1) return 1;
    int maxPrime = 1;
    while (n % 2 == 0) {
        maxPrime = 2;
        n /= 2;
    }
    for (int i = 3; i * i <= n; i += 2) {
        while (n % i == 0) {
            maxPrime = i;
            n /= i;
        }
    }
    if (n > 1) maxPrime = n;
    return maxPrime;
}

class TMaximumPrimeDevisorActor : public TActorBootstrapped<TMaximumPrimeDevisorActor> {
    const TActorId WriteActor;

public:
    TMaximumPrimeDevisorActor(TActorId writeActor) : WriteActor(writeActor) {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateWork);
    }

    void StateWork(TAutoPtr<IEventHandle>& ev) {
        switch (ev->GetTypeRewrite()) {
            case TEvInputNumber::EventType: {
                auto* msg = ev->Get<TEvInputNumber>();
                int result = MaxPrimeDivisor(msg->Value);
                Send(WriteActor, new TEvPrimeDivisor(result));
                break;
            }
            case TEvInputFinished::EventType: {
                Send(WriteActor, new TEvInputFinished());
                PassAway();
                break;
            }
        }
    }
};

IActor* CreateMaxPrimeActor(TActorId writeActor) {
    return new TMaximumPrimeDevisorActor(writeActor);
}


class TWriteActor : public TActorBootstrapped<TWriteActor> {
    int Sum = 0;

public:
    void Bootstrap() {
        Become(&TWriteActor::StateWork);
    }

    void StateWork(TAutoPtr<IEventHandle>& ev) {
        switch (ev->GetTypeRewrite()) {
            case TEvPrimeDivisor::EventType: {
                auto* msg = ev->Get<TEvPrimeDivisor>();
                Sum += msg->Value;
                break;
            }
            case TEvInputFinished::EventType: {
                std::cout << Sum << std::endl;
                PassAway();
                break;
            }
        }
    }
};

IActor* CreateWriteActor() {
    return new TWriteActor();
}
