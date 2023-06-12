//
// Created by Mi on 11.06.23.
//
#ifndef ACTOR_MODEL_ERATOSPHENE_SIEVE_H
#define ACTOR_MODEL_ERATOSPHENE_SIEVE_H

class eratosphene_sieve {
private:
    std::vector<bool> prime;
    int n;
public:
    eratosphene_sieve()() {
        selfResize(10);
    }

    void selfResize(int n){
        n = n + 1;
        prime.resize(n, true);
        prime[0] = prime[1] = false;
        for (int i = 2; i <= n; i++) {
            if (prime[i])
                if(i * 1ll * i <= n)
                    for(int j = i * i; j <=n; j += i)
                        prime[j] = false;
        }
    }

    int getN(){
        return n;
    }

    bool checkPrime(int value){
        return prime[value];
    }
};


#endif //ACTOR_MODEL_ERATOSPHENE_SIEVE_H
