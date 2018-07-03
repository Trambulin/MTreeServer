#include<netinet/in.h>
#include<unistd.h>
#include"jobManager.hpp"
#include"applog.hpp"

std::vector<poolConnecter*> jobManager::pools;
std::vector<pthread_t> jobManager::poolThreads;
std::vector<clientConnecter*> jobManager::clients;
std::vector<pthread_t> jobManager::clientThreads;

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