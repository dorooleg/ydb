//
// Created by postgres2 on 09.06.23.
//

#ifndef ACTOR_MODEL_THESIEVEOFERATOSTHENES_H
#define ACTOR_MODEL_THESIEVEOFERATOSTHENES_H

class TheSieveOfEratosthenes {
private:
    std::vector<bool> prime;
public:
    TheSieveOfEratosthenes() {
        int n = std::numeric_limits<int>::max() - 1000;
        prime.resize(n, true);
        prime[0] = prime[1] = false;
        for (int i = 2; i <= n; i++) {
            if (prime[i])
                if(i * 1ll * i <= n)
                    for(int j = i * i; j <=n; j += i)
                        prime[j] = false;
        }
    }
    void toString(){
        for(int i = 1; i <= 10; i++){
            std::cout << prime[i] << " ";
        }
        std::cout << "\n";
    }

    bool checkPrime(int value){
        return prime[value];
    }
};


#endif //ACTOR_MODEL_THESIEVEOFERATOSTHENES_H
