#ifndef CLIENTCONNECTER_H
#define CLIENTCONNECTER_H

#include<netinet/in.h>

class clientConnecter
{
private:
    int sockfd;
    bool halfLength;
    int16_t fullMContLength, recurContLengthSummer, currentContLength;
    char clientIP[16];
    char buffer[256];
    char* fullMsgContainer;

    void messageHandler();
    void bufferCopy(int bufLength, int startIndex, int actualBufLength);

public:
    clientConnecter(int sockfd, in_addr_t cIP);
    ~clientConnecter();

    void bufferMsgCheck(int bufLength, bool recurCall=false);

    bool connOver;

    static void* initClient(void *clientConn);
    int sendMessage(char* buffer, size_t length);
};

#endif