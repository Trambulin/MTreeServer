#include<stdio.h>
//#include<stdlib.h>
#include<string.h>
#include<unistd.h>
//#include<sys/types.h> 
//#include<sys/socket.h>
#include"clientConnecter.hpp"
#include"applog.hpp"

clientConnecter::clientConnecter(int sockfd, in_addr_t cIP)
{
    connOver=false;
    this->sockfd=sockfd;
    unsigned char ip1=cIP>>24;
    unsigned char ip2=cIP>>16;
    unsigned char ip3=cIP>>8;
    unsigned char ip4=cIP;
    sprintf(clientIP, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
}

clientConnecter::~clientConnecter()
{
    close(sockfd);
}

void* clientConnecter::initClient(void *clientConn)
{
    clientConnecter *tClient=(clientConnecter*)clientConn;
    int n;
    char *idContainer;
    idContainer=new char[5];
    sprintf(idContainer,"%d",tClient->sockfd);
    memset(tClient->buffer,0,256);
    //bzero(buffer,256);
    while(1){
        n = read(tClient->sockfd,tClient->buffer,255);
        if (n < 0){
            applog::log(LOG_ERR,"ERROR reading from socket: ");
            applog::log(LOG_ERR,tClient->clientIP);
        }
        applog::log(LOG_INFO,idContainer);
        applog::log(LOG_INFO,tClient->buffer);
        applog::log(LOG_ERR,tClient->clientIP);
        if(tClient->connOver){
            break;
        }
    }
    n = write(tClient->sockfd,"Connection over",16);
    if (n < 0){
        applog::log(LOG_ERR,"ERROR writing to socket");
    }
    delete tClient;
}

int clientConnecter::sendMessage(char* buffer, size_t length)
{
    int n;
    n = write(sockfd, buffer, length);
    if (n < 0){
        applog::log(LOG_ERR,"ERROR writing to socket: ");
        applog::log(LOG_ERR,clientIP);
    }
    return n;
}