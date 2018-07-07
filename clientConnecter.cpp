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
    connOver=false; halfLength=false;
    fullMContLength=0; recurContLengthSummer=0;
    this->sockfd=sockfd;
    unsigned char ip1=cIP>>24;
    unsigned char ip2=cIP>>16;
    unsigned char ip3=cIP>>8;
    unsigned char ip4=cIP;
    sprintf(clientIP, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
}

clientConnecter::~clientConnecter()
{
    if(fullMContLength>0)
        delete fullMsgContainer;
    close(sockfd);
}

void clientConnecter::messageHandler()
{
    if(fullMContLength<1){
        applog::log(LOG_NOTICE,"0 byte msg");
    }
    else{
        applog::log(LOG_NOTICE,"One message appeared:");
        char* correctDisplay=new char[fullMContLength+1];
        memcpy(correctDisplay,fullMsgContainer,fullMContLength);
        correctDisplay[fullMContLength]='\0';
        applog::log(LOG_NOTICE,correctDisplay);
    }
}

void clientConnecter::bufferCopy(int bufLength, int startIndex, int actualBufLength)
{
    if(actualBufLength<fullMContLength+3){ //+3 = 2byte length+ 1 byte checksum
        //full msg will come with another read
        for(int i=startIndex;i<bufLength;i++){
            fullMsgContainer[currentContLength++]=buffer[i];
        }
    }
    else if(actualBufLength>fullMContLength+3){
        //there is more than 1 msg in the buffer
        int8_t checksum=0;
        for(int i=startIndex;i<startIndex+fullMContLength;i++){
            fullMsgContainer[currentContLength++] = buffer[i];
            checksum+=buffer[i];
        }
        if(checksum != buffer[startIndex+fullMContLength]){
            applog::log(LOG_ERR,"checksum hiba");
        }
        messageHandler();
        bufferMsgCheck(bufLength,true);
    }
    else{
        //exactly 1 msg in the buffer
        int8_t checksum=0;
        for(int i=startIndex;i<bufLength-1;i++){
            fullMsgContainer[currentContLength++] = buffer[i];
            checksum+=buffer[i];
        }
        if(checksum != buffer[bufLength-1]){
            applog::log(LOG_ERR,"checksum hiba");
        }
        messageHandler();
        fullMContLength=0; currentContLength=0;
        delete[] fullMsgContainer;
    }
}

void clientConnecter::bufferMsgCheck(int bufLength, bool recurCall/*=false*/)
{
    int startIndex=0;
    int actualBufLength=bufLength;
    if(recurCall){
        //it will fail if more than 1 recurCall happens
        //need fullMContLength summer in order to make multiple recall
        if(halfLength)
            recurContLengthSummer+=fullMContLength+2;
        else
            recurContLengthSummer+=fullMContLength+3;
        halfLength=false;
        startIndex=recurContLengthSummer;
        actualBufLength=bufLength-recurContLengthSummer;
        fullMContLength=0; currentContLength=0;
        delete[] fullMsgContainer;
    }
    if(halfLength){
        if(bufLength<1){
            //0 byte read, conn over?
        }
        else{
            fullMContLength+=buffer[0];
            fullMsgContainer=new char[fullMContLength];
            bufferCopy(bufLength, startIndex+1, bufLength+1);
        }
        halfLength=false;
    }
    else if(fullMContLength>0){
        //part of the msg is ready, can't be here on recurCall
        if((fullMContLength-currentContLength)+1 > bufLength){
            for(int i=startIndex;i<bufLength;i++){
                fullMsgContainer[currentContLength++]=buffer[i];
            }
        }
        else if((fullMContLength-currentContLength)+1 < bufLength){
            int currTemp=currentContLength;
            for(int i=startIndex;i<fullMContLength-currTemp;i++){
                fullMsgContainer[currentContLength++]=buffer[i];
            }
            int8_t checksum=0;
            for(int i=0;i<fullMContLength;i++){
                checksum+=fullMsgContainer[i];
            }
            if(checksum != buffer[fullMContLength-currTemp]){
                applog::log(LOG_ERR,"checksum hiba");
            }
            messageHandler();
            fullMContLength=fullMContLength-currTemp-2;
            bufferMsgCheck(bufLength,true);
        }
        else{
            for(int i=startIndex;i<bufLength-1;i++){
                fullMsgContainer[currentContLength++] = buffer[i];
            }
            int8_t checksum=0;
            for(int i=0;i<fullMContLength;i++){
                checksum+=fullMsgContainer[i];
            }
            if(checksum != buffer[bufLength-1]){
                applog::log(LOG_ERR,"checksum hiba");
            }
            messageHandler();
            fullMContLength=0; currentContLength=0;
            delete[] fullMsgContainer;
        }
    }
    else{
        if(actualBufLength < 2){
            //cant determine msg length with that
            if(actualBufLength==1){
                fullMContLength=buffer[startIndex]<<8;
                halfLength=true;
            }
            else{
                //read with 0 byte (conn over?)
            }
        }
        else{
            fullMContLength=buffer[startIndex]<<8;
            fullMContLength+=buffer[startIndex+1];
            fullMsgContainer=new char[fullMContLength];
            bufferCopy(bufLength, startIndex+2, actualBufLength);
        }
    }
    recurContLengthSummer=0;
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
        else{
            tClient->bufferMsgCheck(n);
        }
        applog::log(LOG_INFO,idContainer);
        //applog::log(LOG_INFO,tClient->buffer);
        //applog::log(LOG_ERR,tClient->clientIP);
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