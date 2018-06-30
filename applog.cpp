#include<iostream>
#include"applog.hpp"

std::ofstream applog::logFile;
pthread_mutex_t applog::applogLock;

void applog::init()
{
    pthread_mutex_init(&applog::applogLock, NULL);
}

void applog::log(int prio, const char *msg)
{
    pthread_mutex_lock(&applog::applogLock);
    try{
        logFile.open("test.txt",std::ofstream::out | std::ofstream::app);
        logFile << msg << "\n";
    }
    catch (...) {
        std::cout << "Exception opening/reading file";
    }
    logFile.close();
    //std::cout << msg;
    pthread_mutex_unlock(&applog::applogLock);
}