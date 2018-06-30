#ifndef APPLOG_H
#define APPLOG_H

#include<pthread.h>
#include<fstream>

enum {
	LOG_ERR,
	LOG_WARNING,
	LOG_NOTICE,
	LOG_INFO,
	LOG_DEBUG,
	/* custom notices */
	LOG_BLUE = 0x10,
};

class applog
{
public:
	static std::ofstream logFile;
    static pthread_mutex_t applogLock;
    static void log(int prio, const char *msg);
    static void init();

private:
    applog(){}
};

#endif