#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <library/cpp/actors/core/actor.h>
#include <library/cpp/actors/core/events.h>
#include <util/system/datetime.h>
#include <cmath>
#include <random>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {

public:
    TReadActor(std::istream& input, NActors::TActorId writer) : inputStream(input), writeActorId(writer) {}
    std::istream& inputStream;
    NActors::TActorId writeActorId;
    int activeActors = 0;
    bool finishedReading = false;

    void Bootstrap() {
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        Become(&TReadActor::StateFunc);
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        cFunc(TEvents::TEvDone::EventType, HandleFinished);
    });

    void HandleWakeup() {
        int64_t number;
        while (inputStream >> number) {
            Register(CreateMaximumPrimeDevisorActor(number, SelfId(), writeActorId).Release());
            ++activeActors;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        }
        finishedReading = true;
    }

    void HandleFinished() {
        --activeActors;
        if (finishedReading && activeActors <= 0) {
            Send(writeActorId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            PassAway();
        }
    }
};

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {

public:
    TMaximumPrimeDevisorActor(int64_t num, NActors::TActorId reader, NActors::TActorId writer)
        : value(num), readActorId(reader), writeActorId(writer) {}
    
    int64_t value;
    NActors::TActorId readActorId;
    NActors::TActorId writeActorId;
    int64_t currentDivisor = 1;
    int64_t maxPrimeDivisor = 0;

    void Bootstrap() {
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        Become(&TMaximumPrimeDevisorActor::StateFunc);
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
        TInstant startTime = TInstant::Now();
        
        while (currentDivisor <= value) {
            if (value % currentDivisor == 0 && IsPrime(currentDivisor)) {
                maxPrimeDivisor = currentDivisor;
            }
            currentDivisor++;
            if ((TInstant::Now() - startTime).MilliSeconds() >= 10) {
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                return; // Exit the function to avoid further processing in this cycle
            }
        }
        
        Send(writeActorId, std::make_unique<TEvents::TEvWriteValueRequest>(maxPrimeDivisor));
        Send(readActorId, std::make_unique<TEvents::TEvDone>());
        PassAway();
    }

    static bool IsPrime(int64_t n) {
        if (n <= 1) return false;
        if (n <= 3) return true;
        if (n % 2 == 0 || n % 3 == 0) return false;

        // Use Fermat's little theorem to check for primality
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int64_t> dis(2, n - 2);

        for (int i = 0; i < 20; ++i) {
            int64_t a = dis(gen);
            if (PowerMod(a, n - 1, n) != 1) {
                return false;
            }
        }

        return true;
    }

    static int64_t PowerMod(int64_t base, int64_t exp, int64_t mod) {
        int64_t result = 1;
        base = base % mod;

        while (exp > 0) {
            if (exp % 2 == 1)
                result = (result * base) % mod;

            exp = exp >> 1; // exp /= 2
            base = (base * base) % mod;
        }

        return result;
    }
};

class TWriteActor : public NActors :: T Actor<TWrite Actor> {

public:
     TWrite Actor () : T Actor(&TWrite Actor :: State Func ){}
    
     int64t total Sum=0;

     STRICT_STFUNC(State Func ,{
         cFunc(N Actors :: T Events :: TEv PoisonPill :: EventType , Handle PoisonPill );
         hFunc(T Events :: TEv Write Value Request , Handle Write Value Request );
     });

     void Handle Write Value Request(T Events :: TEv Write Value Request :: TPtr event ) { 
         total Sum += event -> Get () -> Value ;
     }

     void Handle PoisonPill () { 
         Cout << total Sum << Endl ;
         Get Program Should Continue () -> ShouldStop ();
         PassAway ();
     }
};

class TSelf Ping Actor : public N Actors :: T Actor Bootstrapped <TSelf Ping Actor > { 
     T Duration Latency ;
     T Instant Last Time ;

public :
     TSelf Ping Actor (const T Duration & latency )
         : Latency (latency )
     {}

     void Bootstrap () { 
         Last Time = T Instant :: Now ();
         Become (&TSelf Ping Actor :: State Func );
         Send (Self Id (), std :: make_unique <N Actors :: T Events :: TEv Wakeup >());
     }

     STRICT_STFUNC(State Func ,{
         cFunc(N Actors :: T Events :: TEv Wakeup :: EventType , Handle Wakeup );
     });

     void Handle Wakeup () { 
         auto now = T Instant :: Now ();
         T Duration delta = now - Last Time ;
         Y_VERIFY(delta <= Latency , "Latency too big" );
         Last Time = now ;
         Send (Self Id (), std :: make_unique <N Actors :: T Events :: TEv Wakeup >());
     }
};

THolder <N Actors :: I Actor > Create Self Ping Actor (const T Duration & latency ) { 
     return Make Holder <TSelf Ping Actor >(latency );
}

THolder <N Actors :: I Actor > Create Write Actor () { 
     return Make Holder <T Write Actor >();
}

THolder <N Actors :: I Actor > Create Read Actor (std :: istream & strm , N Actors :: T Actor Id recipient ) { 
     return Make Holder <T Read Actor >(strm , recipient );
}

THolder <N Actors :: I Actor > Create Maximum Prime Devisor Actor (int64t value , N Actors :: T Actor Id read Actor Id , N Actors :: T Actor Id recipient ) { 
     return Make Holder <T Maximum Prime Devisor Actor >(value , read Actor Id , recipient );
}

std:shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() { 
     return Should Continue ;
}
