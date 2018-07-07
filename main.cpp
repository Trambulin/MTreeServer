#include<iostream>
#include<unistd.h>
#include"poolConnecter.hpp"
#include"jobManager.hpp"
#include"applog.hpp"

int main(int argc, char** argv)
{
    applog::init();
    applog::logFile.open("test.txt"); //emptying file
    applog::logFile.close();
    char url[29]="http://dash.suprnova.cc:9995";
    char user[15]="trambulin.test";
    char pass[2]="x";
    int n;
    pthread_t pthClient;
    pthread_attr_t attrClient;
    /*char url[29]="http://ltc.suprnova.cc:4444";
    char user[15]="trambulin.tram";
    char pass[2]="x";*/
    //jobManager::startNewPoolConnection(url,user,pass);
    int port=27015;
    
    pthread_attr_init(&attrClient);
    n = pthread_create(&pthClient, &attrClient, jobManager::startClientListen, &port);
    pthread_attr_destroy(&attrClient);
    if(n) {
        applog::log(LOG_ERR,"jobManager thread create failed");
    }
    char f[256];
    std::cout << "teszt";
    while(1){
        std::cin >> f;
        std::cout << f << "\n";
        //jobManager::clients[1]->connOver=true; //close connection
        //jobManager::clients[1]->sendMessage(f,10);
    }
    return 0;
}