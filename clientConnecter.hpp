#ifndef CLIENTCONNECTER_H
#define CLIENTCONNECTER_H

#include<netinet/in.h>

class clientConnecter
{
private:
    int sockfd;
    char clientIP[16];
    char buffer[256];

public:
    clientConnecter(int sockfd, in_addr_t cIP);
    ~clientConnecter();

    bool connOver;

    static void* initClient(void *clientConn);
    int sendMessage(char* buffer, size_t length);
};

#endif