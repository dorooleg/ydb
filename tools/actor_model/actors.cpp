#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <stdio.h>
#include <time.h>

using namespace std;

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

/*
Вам нужно написать реализацию TReadActor, TMaximumPrimeDevisorActor, TWriteActor
*/

/*
Требования к TReadActor:
+1. Рекомендуется отнаследовать этот актор от NActors::TActorBootstrapped
+2. В Boostrap этот актор отправляет себе NActors::TEvents::TEvWakeup
+3. После получения этого сообщения считывается новое int64_t значение из strm
+4. После этого порождается новый TMaximumPrimeDevisorActor который занимается вычислениями
+5. Далее актор посылает себе сообщение NActors::TEvents::TEvWakeup чтобы не блокировать поток этим актором
+6. Актор дожидается завершения всех TMaximumPrimeDevisorActor через TEvents::TEvDone
+7. Когда чтение из файла завершено и получены подтверждения от всех TMaximumPrimeDevisorActor,
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
    int  numberCount;    
    const NActors::TActorId WriteActor;	
public:
    TReadActor(const NActors::TActorId writeActor)
        	: numberCount(0), WriteActor(writeActor)
    { }

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, Handle_TEvWakeup);
        cFunc(TEvents::TEvDone::EventType, Handle_TEvDone);
    });

    void Handle_TEvWakeup() {

    	int64_t number;
    	int ret = fscanf(stdin,"%ld", &number);
    	//printf("fscanf_d: %d, %ld\n",ret,number);
    	
		if (ret < 1 ) {
			if( numberCount == 0 )  
				Send(WriteActor, make_unique<NActors::TEvents::TEvPoisonPill>());
			return;
		}

		numberCount++;
		Register(CreateTMaximumPrimeDevisorActor(number, SelfId(), WriteActor).Release());
		Send(SelfId(), make_unique<NActors::TEvents::TEvWakeup>());
    }
    
    void Handle_TEvDone() {
       numberCount--;
       //printf("Handle_TEvDone: %d\n",numberCount);
       if (numberCount == 0) {
            Send(WriteActor, make_unique<NActors::TEvents::TEvPoisonPill>());
        }
    }
};
THolder<NActors::IActor> CreateTReadActor(const NActors::TActorId writeActor) {
    return MakeHolder<TReadActor>(writeActor);
}

// TReadActor -------------------------------------------------------


/*
Требования к TMaximumPrimeDevisorActor:
+1. Рекомендуется отнаследовать этот актор от NActors::TActorBootstrapped
+2. В конструкторе этот актор принимает:
 - значение для которого нужно вычислить простое число
 - ActorId отправителя (ReadActor)
 - ActorId получателя (WriteActor)
+2. В Boostrap этот актор отправляет себе NActors::TEvents::TEvWakeup по вызову которого происходит вызов Handler для вычислений
+3. Вычисления нельзя проводить больше 10 миллисекунд
+4. По истечении этого времени нужно сохранить текущее состояние вычислений в акторе и отправить себе NActors::TEvents::TEvWakeup
+5. Когда результат вычислен он посылается в TWriteActor c использованием сообщения TEvWriteValueRequest
+6. Далее отправляет ReadActor сообщение TEvents::TEvDone
+7. Завершает свою работу

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
class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t number;
    const NActors::TActorIdentity ReadActor;
    const NActors::TActorId WriteActor;
    int64_t maxPrime;	
    int64_t curPrime;
    int64_t curNumber;
    	
public:
    TMaximumPrimeDevisorActor(int64_t number, const NActors::TActorIdentity readActor, const NActors::TActorId writeActor)
        : number(number), ReadActor(readActor), WriteActor(writeActor), maxPrime(number),  curPrime(2),  curNumber(number)
    {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, Handle_TEvWakeup);
    });

    void Handle_TEvWakeup() {
		if( number > 0 )
		{
			struct timespec t1,t2;
			clock_gettime(CLOCK_REALTIME, &t1);
			while (curNumber != 1)
			{
				if (curNumber % curPrime == 0)
				{
					maxPrime = curPrime;
					curNumber /= curPrime;
				}
				else 
				if (curPrime*curPrime > curNumber)
					curPrime = curNumber;
				else
					curPrime++;

				clock_gettime(CLOCK_REALTIME, &t2);
				if( DiffTimeInNanoSeconds(&t1,&t2) > 10*1000 )
				{
					Send(SelfId(), make_unique<NActors::TEvents::TEvWakeup>());
					return; 
				}
			}
		}
		else
			maxPrime=0;
		    
        Send(WriteActor, make_unique<TEvents::TEvWriteValueRequest>(maxPrime));
        Send(ReadActor, make_unique<TEvents::TEvDone>());
        PassAway();
    }
    

	long DiffTimeInNanoSeconds(struct timespec *t1,struct timespec *t2)
	{
	   time_t  sec;
	   long ns;

	   sec=(time_t)(t2->tv_sec-t1->tv_sec);
	   ns=t2->tv_nsec-t1->tv_nsec;
	   if( ns < 0 )
	   {
		  sec--;
		  ns+=1000000;
	   }
	   if( sec < 0 )
		  sec+=60;

	   return (long)sec*1000000+ns;
	}    
    
};
THolder<NActors::IActor> CreateTMaximumPrimeDevisorActor(int64_t number, const NActors::TActorIdentity readActor, const NActors::TActorId writeActor) {
    return MakeHolder<TMaximumPrimeDevisorActor>(number, readActor, writeActor);
}
// TMaximumPrimeDevisorActor ----------------------------------------------------------------



/*
Требования к TWriteActor:
+1. Рекомендуется отнаследовать этот актор от NActors::TActor
+2. Этот актор получает два типа сообщений NActors::TEvents::TEvPoisonPill::EventType и TEvents::TEvWriteValueRequest
+2. В случае TEvents::TEvWriteValueRequest он принимает результат посчитанный в TMaximumPrimeDevisorActor и прибавляет его к локальной сумме
+4. В случае NActors::TEvents::TEvPoisonPill::EventType актор выводит в Cout посчитанную локальнкую сумму, проставляет ShouldStop и завершает свое выполнение через PassAway

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
    int Sum;
public:
    TWriteActor() : NActors::TActor<TWriteActor>(&TWriteActor::Handler), Sum(0) {}
	
    STRICT_STFUNC(Handler, {
        hFunc(TEvents::TEvWriteValueRequest, Handle_TEvWriteValueRequest);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, Handle_TEvPoisonPill);
    });
    
    void Handle_TEvWriteValueRequest(TEvents::TEvWriteValueRequest::TPtr& ev) {
        Sum += ev->Get()->value;
    }
   
    void Handle_TEvPoisonPill() {
       Cout << Sum << Endl;
       ShouldContinue->ShouldStop();
       PassAway();
    }		
};
THolder<NActors::IActor> CreateTWriteActor() {
    return MakeHolder<TWriteActor>();
}
// TWriteActor ----------------------------------------------------------



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
