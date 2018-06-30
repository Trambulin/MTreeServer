#include<iostream>
#include<unistd.h>
#include"poolConnecter.hpp"
#include"jobManager.hpp"
#include"applog.hpp"

int main(int argc, char** argv)
{
    applog::init();
    //poolConnecter moneroPool;
    //moneroPool.poolMainMethod();
    applog::logFile.open("test.txt"); //emptying file
    applog::logFile.close();
    int port=27015;
    pthread_t pth;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    int n = pthread_create(&pth, &attr, jobManager::startClientListen, &port);
    pthread_attr_destroy(&attr);
    if(n) {
        applog::log(LOG_ERR,"jobManager thread create failed");
    }
    char f[256];
    std::cout << "teszt";
    while(1){
        std::cin >> f;
        std::cout << f << "\n";
        jobManager::clients[1]->connOver=true; //close connection
        //jobManager::clients[1]->sendMessage(f,10);
    }
    return 0;
}