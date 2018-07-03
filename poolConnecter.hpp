#ifndef POOLCONNECTER_H
#define POOLCONNECTER_H

#include<stdint.h>
#include<curl/curl.h>
#include<jansson.h>

#define USER_AGENT "complexMiner 1.0"
#define socket_blocks() (errno == EAGAIN || errno == EWOULDBLOCK)
#define RBUFSIZE 2048
#define RECVSIZE (RBUFSIZE - 4)

class poolConnecter
{
private:
    CURL *curl;
    curl_socket_t sock;
    char *sockBuf;
    size_t sockBufSize;
    char *url, *user, *pass;
    char errorBuf[256];
    char* sessionId;
    int retryCount;
    bool jsonrpc2;
    uint32_t acceptedCount, rejectedCount;

    size_t rpc2Bloblen;
    char *rpc2Blob, *rpc2JobId;
    uint32_t rpc2Target[8];
    double stratumDiff;

    size_t xnonce1Size, xnonce2Size, coinbaseSize;
    int blockHeight, merkleCount, jClean;
    double nextDiff, jDiff; //both are the same?
    char* jobId, rpc2Id[64];
    unsigned char *xnonce1, *xnonce2, *coinbase, **jMerkle, jVersion[4], jNbits[4], jNtime[4], prevHash[32], jClaim[32];

    bool stratumConnect();
    bool stratumSubscribe();
    bool stratumAuthorize();

    bool sendLine(char *s);
    char* receiveLine();
    const char* getStratumSessionId(json_t *val);
    bool handleStratumMethod(const char* s);
    bool handleStratumResponse(char* buf);
    int shareResult(int result, const char *reason);
    bool socketFull(int timeout);
    static int sockKeepaliveCallback(void *userdata, curl_socket_t fd, curlsocktype purpose);
    static curl_socket_t opensockGrabCallback(void *clientp, curlsocktype purpose, struct curl_sockaddr *addr);

    bool rpc2LoginDecode(const json_t *val);
    bool rpc2JobDecode(const json_t *params);
    bool stratumNotify(json_t *params);
    bool getStratumExtranonce(json_t *val,int pndx);
    bool stratumStats(json_t *id, json_t *params);
    uint32_t getblocheight();
    bool hex2bin(unsigned char *p, const char *hexstr, size_t len);

public:
    poolConnecter();
    poolConnecter(char *stratumUrl, char *startUser, char *startPass);
    ~poolConnecter();

    static void* poolMainMethod(void *poolConnObject);
};

#endif