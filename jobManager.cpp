#include<netinet/in.h>
//#include<netinet/tcp.h>
#include<unistd.h>
#include"jobManager.hpp"
#include"applog.hpp"

std::vector<poolConnecter*> jobManager::pools;
std::vector<pthread_t> jobManager::poolThreads;
std::vector<clientConnecter*> jobManager::clients;
std::vector<pthread_t> jobManager::clientThreads;

void jobManager::notifyClients(char* buffer, size_t length)
{
    for(int i=0;i<clients.size();i++){
        if(clients[i]->isDestructible){
            delete clients[i];
            clients.erase(clients.begin()+i);
            clientThreads.erase(clientThreads.begin()+i);
            i--; //client size is reduced, i must be reduced to not miss a client
        }
        else{
            clients[i]->sendMessage(buffer, length);
        }
    }
}

void *jobManager::startClientListen(void *port)
{
    int sockfd, newsockfd, n;
    int *sockPort=(int*)port;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0){
        applog::log(LOG_ERR,"ERROR opening socket");
        exit(1);
    }
    
    //it is said, it is better to do keepalive with accepted clients, but it works here as well
    /* int keepAlive=1, kAIdle=20, kAInterval=5, kACount=3;
    if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(keepAlive)) < 0){
        applog::log(LOG_ERR,"Keepalive failed");
    }
    if (keepAlive)
    {
        if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &kAIdle, sizeof(kAIdle)) < 0){
            applog::log(LOG_ERR,"KeepIdle failed");
        }
        if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &kAInterval, sizeof(kAInterval)) < 0){
            applog::log(LOG_ERR,"KeepInterval failed");
        }
        if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, &kACount, sizeof(kACount)) < 0){
            applog::log(LOG_ERR,"KeepCount failed");
        }
    } */

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(*sockPort);
    if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
        applog::log(LOG_ERR,"ERROR on binding");
        exit(1);
    }
    listen(sockfd,5);
    clilen = sizeof(cli_addr);
    while(1){
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0){
            applog::log(LOG_ERR,"ERROR on client accept");
        }
        clients.push_back(new clientConnecter(newsockfd, cli_addr.sin_addr.s_addr));
        pthread_t pth;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        n = pthread_create(&pth, &attr, clientConnecter::initClient, clients.back());
        pthread_attr_destroy(&attr);
        if(n) {
            clientConnecter *deleter=clients.back();
            delete deleter;
            clients.pop_back();
            applog::log(LOG_ERR,"client thread create failed");
            continue;
        }
        clientThreads.push_back(pth);
    }
}

bool jobManager::startNewPoolConnection(char *url, char *user, char *pass)
{
    int n;
    pthread_t pth;
    pthread_attr_t attr;
    poolConnecter *somePool;
    somePool = new poolConnecter(url, user, pass);
    pthread_attr_init(&attr);
    n = pthread_create(&pth, &attr, poolConnecter::poolMainMethod, somePool);
    pthread_attr_destroy(&attr);
    if(n) {
        delete somePool;
        applog::log(LOG_ERR,"client thread create failed");
        return false;
    }
    pools.push_back(somePool);
    poolThreads.push_back(pth);
    return true;
}