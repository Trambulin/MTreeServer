#ifndef JOBMANAGER_H
#define JOBMANAGER_H

#include<vector>
#include"poolConnecter.hpp"
#include"clientConnecter.hpp"

//static class?
class jobManager
{
private:
    jobManager();

public:
    static std::vector<poolConnecter*> pools;
    static std::vector<pthread_t> poolThreads;
    static std::vector<clientConnecter*> clients;
    static std::vector<pthread_t> clientThreads;

    static void calculateClientRanges();
    static void notifyClients(char* buffer, size_t length);

    static void *startClientListen(void *port);
    static bool startNewPoolConnection(char *url, char *user, char *pass);
};

#endif