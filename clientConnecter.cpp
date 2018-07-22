#include<stdio.h>
//#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<netinet/tcp.h>
//#include<sys/types.h> 
//#include<sys/socket.h>
#include"clientConnecter.hpp"
#include"applog.hpp"

clientConnecter::clientConnecter(int sockfd, in_addr_t cIP)
{
    connOver=false; halfLength=false; isDestructible=false;
    hashPerSec=0; isReady=false;
    //keepAlive=1; kAIdle=20; kAInterval=5; kACount=3;
    currentContLength=0; fullMContLength=0; recurContLengthSummer=0;
    this->sockfd=sockfd;
    pthread_mutex_init(&publicOverLock, NULL);
    pthread_mutex_init(&publicHpSLock, NULL);
    unsigned char ip1=cIP>>24;
    unsigned char ip2=cIP>>16;
    unsigned char ip3=cIP>>8;
    unsigned char ip4=cIP;
    sprintf(clientIP, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
}

clientConnecter::~clientConnecter()
{
    pthread_mutex_destroy(&publicOverLock);
    pthread_mutex_destroy(&publicHpSLock);
    if(fullMContLength>0)
        delete[] fullMsgContainer;
    close(sockfd);
}

inline void clientConnecter::uint16ToChar2(uint16_t src, char dest[2])
{
    dest[0] = (unsigned char)(src >> 8);
	dest[1] = (unsigned char)src;
}

inline uint16_t clientConnecter::char2ToUint16(char src[2])
{
    uint16_t result=0;
    result=src[0]<<8;
    result+=(unsigned char)src[1];
    return result;
}

void clientConnecter::askForSpeed()
{
    char askSpeedMsg[3];
    uint16ToChar2(3,askSpeedMsg);
    askSpeedMsg[2]=0; //scrypt algInd
    sendMessage(askSpeedMsg, 3);
}

void clientConnecter::registrationMsg()
{
    std::string textUP(fullMsgContainer+2,fullMContLength-2);
    size_t dividerIndex=textUP.find_first_of((char)1);
    std::string user=textUP.substr(0,dividerIndex+1);
    std::string pass=textUP.substr(dividerIndex+1);
    //TODO register user with the above details
    char *result=new char[3];
    uint16ToChar2(1,result);
    if(true)
        result[2]=0; //registration success
    else
        result[2]=1; //registration failed
    sendMessage(result,3);
    delete[] result;
}

void clientConnecter::loginMsg()
{
    std::string textUP(fullMsgContainer+2,fullMContLength-2);
    size_t dividerIndex=textUP.find_first_of((char)1);
    std::string user=textUP.substr(0,dividerIndex+1);
    std::string pass=textUP.substr(dividerIndex+1);
    //TODO login user with the above details
    char *result=new char[3];
    uint16ToChar2(2,result);
    if(true)
        result[2]=0; //login success
    else
        result[2]=1; //login failed
    sendMessage(result,3);
    if(true)
        askForSpeed();
    delete[] result;
}

void clientConnecter::processSpeedInfo()
{
    uint8_t algIndex=fullMsgContainer[2];
    uint32_t tHashSpeed = ((uint32_t)((unsigned char)fullMsgContainer[3])) << 24;
    tHashSpeed += ((uint32_t)((unsigned char)fullMsgContainer[4])) << 16;
    tHashSpeed += ((uint32_t)((unsigned char)fullMsgContainer[5])) << 8;
    tHashSpeed += (uint32_t)((unsigned char)fullMsgContainer[6]);
    if(algIndex == 0){ //scrypt
        pthread_mutex_lock(&publicHpSLock);
        hashPerSec=tHashSpeed;
        isReady=true;
        pthread_mutex_unlock(&publicHpSLock);
        applog::log(LOG_NOTICE,"Hash speedinfo APPEARED");
        applog::log(LOG_NOTICE,std::to_string(hashPerSec).c_str());
    }
    else{ //fail
    }
}

//processing the received core (no header/footer) message
void clientConnecter::messageHandler()
{
    if(fullMContLength<3){
        applog::log(LOG_NOTICE,"too short real content");
    }
    else{
        uint16_t msgType= char2ToUint16(fullMsgContainer);
        if(msgType == 0){ //version
            if(fullMsgContainer[2]){ //client's version is not eligible
                pthread_mutex_lock(&publicOverLock);
                connOver=true;
                pthread_mutex_unlock(&publicOverLock);
            }
            applog::log(LOG_NOTICE,"Version check DONE");
        }
        else if(msgType == 1){ //registration
            registrationMsg();
        }
        else if(msgType == 2){ //login
            loginMsg();
        }
        else if(msgType == 3){ //speed HashPerSec
            processSpeedInfo();
        }

        //char* correctDisplay=new char[fullMContLength+1];
        //memcpy(correctDisplay,fullMsgContainer,fullMContLength);
        //correctDisplay[fullMContLength]='\0';
        //applog::log(LOG_NOTICE,"One message appeared:");
        //applog::log(LOG_NOTICE,correctDisplay);
        //delete[] correctDisplay;
    }
}

//just a function to prevent redundancy lines in bufferMsgCheck
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

//pull out/select full template messages from the stream and call messageHandler for every msg
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
        fullMContLength+=buffer[0];
        fullMsgContainer=new char[fullMContLength];
        bufferCopy(bufLength, startIndex+1, bufLength+1);
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
            fullMContLength=buffer[startIndex]<<8;
            halfLength=true;
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

void clientConnecter::sendHashingDatas(uint8_t algIndex, int jobIndex, uint32_t *datas, int datasLength, uint32_t minVal, uint32_t maxVal, uint32_t target)
{
}

void commentholder()
{
    /* if (setsockopt(tClient->sockfd, SOL_SOCKET, SO_KEEPALIVE, &tClient->keepAlive, sizeof(tClient->keepAlive)) < 0)
    {
        //applog::log(LOG_ERR,"Keepalive failed");
        //Error occurred, so output an error message containing:
        //__LINE__, socket, SO_KEEPALIVE, switch, errno, etc.
    }
    if (false)//tClient->keepAlive)
    {
        // Set the number of seconds the connection must be idle before sending a KA probe.
        if (setsockopt(tClient->sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &tClient->kAIdle, sizeof(tClient->kAIdle)) < 0)
        {
            applog::log(LOG_ERR,"KeepIdle failed");
            // Error occurred, so output an error message containing:
            //__LINE__, socket, TCP_KEEPIDLE, idle, errno, etc.
        }

        // Set how often in seconds to resend an unacked KA probe.
        if (setsockopt(tClient->sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &tClient->kAInterval, sizeof(tClient->kAInterval)) < 0)
        {
            applog::log(LOG_ERR,"KeepInterval failed");
            // Error occurred, so output an error message containing:
            //__LINE__, socket, TCP_KEEPINTVL, interval, errno, etc.
        }

        // Set how many times to resend a KA probe if previous probe was unacked.
        if (setsockopt(tClient->sockfd, IPPROTO_TCP, TCP_KEEPCNT, &tClient->kACount, sizeof(tClient->kACount)) < 0)
        {
            applog::log(LOG_ERR,"KeepCount failed");
            // Error occurred, so output an error message containing:
            //__LINE__, socket, TCP_KEEPCNT, count, errno, etc.
        }
    } */
}

//input message function for read incoming messages (should be renamed?)
void* clientConnecter::initClient(void *clientConn)
{
    clientConnecter *tClient=(clientConnecter*)clientConn;
    int n;
    struct timeval tv;
    tv.tv_sec = 10;  //time before error read occurs
    //read function return -1 every tv seconds, must be set here
    /* if(setsockopt(tClient->sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval))){
        applog::log(LOG_ERR,"Timeo failed");
    } */
    //commentholder()
    char *idContainer;
    idContainer=new char[5];
    sprintf(idContainer,"%d",tClient->sockfd);
    memset(tClient->buffer,0,256);
    char minVersion[6];
    minVersion[0]=0; minVersion[1]=0; //message type
    minVersion[2]=0; minVersion[3]=0; minVersion[4]=0; minVersion[5]=1; //version (int32)
    tClient->sendMessage(minVersion,6);
    //bzero(buffer,256);
    while(1){
        n = read(tClient->sockfd,tClient->buffer,255);
        if (n < 1){
            if(n < 0){
                //occurs on error, or time limit exceeding
                //applog::log(LOG_ERR,"ERROR reading from socket: ");
                //applog::log(LOG_ERR,tClient->clientIP);
                char errBuf[35];
                sprintf(errBuf,"socker read error, number: %d",n);
                applog::log(LOG_ERR,errBuf);
            }
            else{
                applog::log(LOG_NOTICE,"Client closed connection: ");
                applog::log(LOG_NOTICE,tClient->clientIP);
                break;
            }
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
    pthread_mutex_lock(&tClient->publicOverLock);
    tClient->connOver=true;
    tClient->isDestructible=true;
    pthread_mutex_unlock(&tClient->publicOverLock);
    //delete tClient;
}

int clientConnecter::sendMessage(char* realContent, size_t length)
{
    int n;
    if(!connOver){
        char *buffer=new char[length+3];
        uint16ToChar2((uint16_t)(length),buffer);
        memcpy(buffer+2,realContent,length);
        int8_t checksum = 0;
        for (size_t i = 0; i < length; i++) {
            checksum += realContent[i];
        }
        buffer[length+2] = checksum;
        n = write(sockfd, buffer, length+3);
        if (n < 0){
            applog::log(LOG_ERR,"ERROR writing to socket: ");
            applog::log(LOG_ERR,clientIP);
        }
        return n;
    }
    return -1;
}