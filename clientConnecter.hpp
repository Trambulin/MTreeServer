#ifndef CLIENTCONNECTER_H
#define CLIENTCONNECTER_H

#include<string.h>
#include<netinet/in.h>
#include<pthread.h>

class clientConnecter
{
private:
    int sockfd;
    uint32_t currId;
    bool halfLength;
    int16_t fullMContLength, recurContLengthSummer, currentContLength;
    //int keepAlive, kAIdle, kAInterval, kACount;
    std::string userName, startConnDate;
    char clientIP[16];
    char buffer[256];
    char* fullMsgContainer;

    inline void uint16ToChar2(uint16_t src, char dest[2]);
    inline uint16_t char2ToUint16(char src[2]);
    void askForSpeed();
    void registrationMsg();
    void loginMsg();
    void processSpeedInfo();
    void messageHandler();
    void bufferCopy(int bufLength, int startIndex, int actualBufLength);

public:
    static pthread_mutex_t sqlConnectionLock;
    clientConnecter(int sockfd, in_addr_t cIP);
    ~clientConnecter();

    void sendHashingDatas(uint8_t algIndex, int jobIndex, uint32_t *datas, int datasLength, uint32_t minVal, uint32_t maxVal, uint32_t target);
    void bufferMsgCheck(int bufLength, bool recurCall=false);

    uint32_t hashPerSec;
    pthread_mutex_t publicOverLock, publicHpSLock;
    bool connOver, isDestructible, isReady;

    static void* initClient(void *clientConn);
    int sendMessage(char* realContent, size_t length);
};

#endif