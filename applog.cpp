#include<iostream>
#include"applog.hpp"

pthread_mutex_t applog::applogLock;

void applog::init()
{
    pthread_mutex_init(&applog::applogLock, NULL);
}

void applog::log(int prio, const char *msg)
{
    pthread_mutex_lock(&applog::applogLock);
    std::cout << msg;
    pthread_mutex_lock(&applog::applogLock);
}