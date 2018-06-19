#include<stdint.h>
#include<unistd.h>
#include<curl/curl.h>
#include<netinet/tcp.h>
#include<string.h>
#include<errno.h>
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

    size_t xnonce1Size, xnonce2Size, coinbaseSize;
    int blockHeight, merkleCount, jClean;
    double nextDiff, jDiff; //both are the same?
    char* jobId;
    unsigned char *xnonce1, *xnonce2, *coinbase, **jMerkle, jVersion[4], jNbits[4], jNtime[4], prevHash[32], jClaim[32];

    bool stratumConnect();
    bool stratumSubscribe();
    bool stratumAuthorize();

    bool sendLine(char *s);
    char* receiveLine();
    const char* getStratumSessionId(json_t *val);
    bool handleStratumMessage(const char* s);
    bool handleStratumResponse(char* buf);
    bool socketFull(int timeout);
    static int sockKeepaliveCallback(void *userdata, curl_socket_t fd, curlsocktype purpose);
    static curl_socket_t opensockGrabCallback(void *clientp, curlsocktype purpose, struct curl_sockaddr *addr);

    bool stratumNotify(json_t *params);
    bool getStratumExtranonce(json_t *val,int pndx);
    bool stratumStats(json_t *id, json_t *params);
    uint32_t getblocheight();
    bool hex2bin(unsigned char *p, const char *hexstr, size_t len);

public:
    poolConnecter();
    poolConnecter(char *stratumUrl, char *startUser, char *startPass);
    ~poolConnecter();

    void* poolMainMethod();
};