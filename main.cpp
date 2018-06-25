#include<iostream>
#include"poolConnecter.hpp"
#include"applog.hpp"

int main(int argc, char** argv)
{
    applog::init();
    poolConnecter moneroPool;
    moneroPool.poolMainMethod();
    char f;
    pthread_mutex_lock(&applog::applogLock);
    std::cout << "teszt";
    std::cin >> f;
    pthread_mutex_unlock(&applog::applogLock);
    return 0;
}