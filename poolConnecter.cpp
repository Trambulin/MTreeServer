#include<unistd.h>
#include<netinet/tcp.h>
#include<string.h>
#include<errno.h>
#include"poolConnecter.hpp"
#include"applog.hpp"
#include"sha256Abstract.hpp"

inline uint32_t le32dec(const void *pp)
{
	const uint8_t *p = (uint8_t const *)pp;
	return ((uint32_t)(p[0]) + ((uint32_t)(p[1]) << 8) + ((uint32_t)(p[2]) << 16) + ((uint32_t)(p[3]) << 24));
}

poolConnecter::poolConnecter()
{
	acceptedCount=0; rejectedCount=0;
	curl=NULL; sockBuf=NULL; rpc2Blob=NULL; jobId=NULL;
	jsonrpc2=true;
	wJobId=(char*)malloc(1);
	wJobId[0]='\0';
	//memset(rpc2Target, 0xff, sizeof(rpc2Target));
	rpc2Target=0xffffffff;
	retryCount=-1;
	url=new char[50];
	strcpy(url,"http://killallasics.moneroworld.com:5555");
	user=new char[100];
	strcpy(user,"9v4vTVwqZzfjCFyPi7b9Uv1hHntJxycC4XvRyEscqwtq8aycw5xGpTxFyasurgf2KRBfbdAJY4AVcemL1JCegXU4EZfMtaz");
	pass=NULL;
}

poolConnecter::poolConnecter(char* stratumUrl, char *stratUser, char *stratPass, int poolObjId)
{
	poolId=poolObjId;
	acceptedCount=0; rejectedCount=0;
	curl=NULL; sockBuf=NULL; sessionId=NULL; xnonce1=NULL; coinbase=NULL; jMerkle=NULL; jobId=NULL;
	jsonrpc2=false;
	wJobId=(char*)malloc(1);
	wJobId[0]='\0';
	//memset(rpc2Target, 0xff, sizeof(rpc2Target));
	rpc2Target=0xffffffff;
	retryCount=-1;
	url=stratumUrl;
	user=stratUser;
	pass=stratPass;
}

poolConnecter::~poolConnecter()
{
	delete[] url;
	delete[] user;
	if(pass)
		delete[] pass;
}

int poolConnecter::sockKeepaliveCallback(void *userdata, curl_socket_t fd, curlsocktype purpose)
{
	int tcp_keepcnt = 3;
	int tcp_keepintvl = 50;
	int tcp_keepidle = 50;
	int keepalive = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)))
		return 1;
	if (setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &tcp_keepcnt, sizeof(tcp_keepcnt)))
		return 1;
	if (setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &tcp_keepidle, sizeof(tcp_keepidle)))
		return 1;
	if (setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &tcp_keepintvl, sizeof(tcp_keepintvl)))
		return 1;
	return 0;
}

curl_socket_t poolConnecter::opensockGrabCallback(void *clientp, curlsocktype purpose, struct curl_sockaddr *addr)
{
	curl_socket_t *sock = (curl_socket_t*) clientp;
	*sock = socket(addr->family, addr->socktype, addr->protocol);
	return *sock;
}

bool poolConnecter::stratumConnect()
{
	int rc;
	if(curl)
	{
		applog::log(LOG_ERR, "CURL initialization failed");
		curl_easy_cleanup(curl);
	}
	curl = curl_easy_init();
	if(!curl)
	{
		return false;
	}
	if (!sockBuf) {
		sockBuf = new char[RBUFSIZE];
		sockBufSize = RBUFSIZE;
	}
	sockBuf[0] = '\0';
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_FRESH_CONNECT, 1);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuf);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1);
	curl_easy_setopt(curl, CURLOPT_HTTPPROXYTUNNEL, 1);
	curl_easy_setopt(curl, CURLOPT_SOCKOPTFUNCTION, sockKeepaliveCallback);
	curl_easy_setopt(curl, CURLOPT_OPENSOCKETFUNCTION, opensockGrabCallback);
	curl_easy_setopt(curl, CURLOPT_OPENSOCKETDATA, &sock);
	curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1);
	rc = curl_easy_perform(curl);
	if (rc) {
		applog::log(LOG_ERR, "Stratum connection failed: ");
		applog::log(LOG_ERR, errorBuf);
		curl_easy_cleanup(curl);
		curl = NULL;
		return false;
	}
	return true;
}

bool poolConnecter::socketFull(int timeout)
{
	struct timeval tv;
	fd_set rd;

	FD_ZERO(&rd);
	FD_SET(sock, &rd);
	tv.tv_sec = timeout;
	tv.tv_usec = 0;
	if (select((int)(sock + 1), &rd, NULL, NULL, &tv) > 0)
		return true;
	return false;
}

bool poolConnecter::sendLine(char *s)
{
	size_t sent = 0;
	int len;

	len = (int) strlen(s);
	s[len++] = '\n';

	while (len > 0) {
		struct timeval timeout = {0, 0};
		int n;
		fd_set wd;

		FD_ZERO(&wd);
		FD_SET(sock, &wd);
		if (select((int) (sock + 1), NULL, &wd, NULL, &timeout) < 1)
			return false;
		n = send(sock, s + sent, len, 0);
		if (n < 0) {
			if (!socket_blocks())
				return false;
			n = 0;
		}
		sent += n;
		len -= n;
	}

	return true;
}

char* poolConnecter::receiveLine()
{
	ssize_t len, buflen;
	char *tok, *sret = NULL;

	if (!strstr(sockBuf, "\n")) {
		bool ret = true;
		time_t rstart;

		time(&rstart);
		if (!socketFull(60)) {
			applog::log(LOG_ERR, "stratum_recv_line timed out");
			return sret;
		}
		do {
			char s[RBUFSIZE];
			ssize_t n;

			memset(s, 0, RBUFSIZE);
			n = recv(sock, s, RECVSIZE, 0);
			if (!n) {
				ret = false;
				break;
			}
			if (n < 0) {
				if (!socket_blocks() || !socketFull(1)) {
					ret = false;
					break;
				}
			} else
			{
				size_t old, n;
				old = strlen(sockBuf);
				n = old + strlen(s) + 1;
				if (n >= sockBufSize) {
					sockBufSize = n + (RBUFSIZE - (n % RBUFSIZE));
					delete[] sockBuf;
					sockBuf = new char[sockBufSize];
				}
				strcpy(sockBuf + old, s);
			}
		} while (time(NULL) - rstart < 60 && !strstr(sockBuf, "\n"));

		if (!ret) {
			applog::log(LOG_ERR, "stratum_recv_line failed");
			return sret;
		}
	}

	buflen = (ssize_t) strlen(sockBuf);
	tok = strtok(sockBuf, "\n");
	if (!tok) {
		//applog(LOG_ERR, "stratum_recv_line failed to parse a newline-terminated string");
		return sret;
	}
	sret = strdup(tok);
	len = (ssize_t) strlen(sret);

	if (buflen > len + 1)
		memmove(sockBuf, sockBuf + len + 1, buflen - len + 1);
	else
		sockBuf[0] = '\0';
	return sret;
}

const char* poolConnecter::getStratumSessionId(json_t* val)
{
	json_t *arr_val;
	int i, n;

	arr_val = json_array_get(val, 0);
	if (!arr_val || !json_is_array(arr_val))
		return NULL;
	n = (int) json_array_size(arr_val);
	for (i = 0; i < n; i++) {
		const char *notify;
		json_t *arr = json_array_get(arr_val, i);

		if (!arr || !json_is_array(arr))
			break;
		notify = json_string_value(json_array_get(arr, 0));
		if (!notify)
			continue;
		if (!strcasecmp(notify, "mining.notify"))
			return json_string_value(json_array_get(arr, 1));
	}
	return NULL;
}

bool poolConnecter::getStratumExtranonce(json_t* val,int pndx)
{
	const char* xn1;
	int xn2Size;

	xn1 = json_string_value(json_array_get(val, pndx));
	if (!xn1) {
		applog::log(LOG_ERR, "Failed to get extranonce1");
		return false;
	}
	xn2Size = (int) json_integer_value(json_array_get(val, pndx+1));
	if (!xn2Size) {
		applog::log(LOG_ERR, "Failed to get extranonce2_size");
		return false;
	}
	if (xn2Size < 2 || xn2Size > 16) {
		applog::log(LOG_INFO, "Failed to get valid n2size in parse_extranonce");
		return false;
	}
	if (xnonce1)
		free(xnonce1);
	xnonce1Size = strlen(xn1) / 2;
	xnonce1 = (unsigned char*) calloc(1, xnonce1Size);
	if (!xnonce1) {
		applog::log(LOG_ERR, "Failed to alloc xnonce1");
		return false;
	}
	hex2bin(xnonce1, xn1, xnonce1Size);
	xnonce2Size = xn2Size;
	//some debug log for pool dynamic change
	return true;
}

bool poolConnecter::stratumSubscribe()
{
	char *s, *sret = NULL;
	bool ret = false;
	const char *sid;
	json_t *val = NULL, *res_val, *err_val;
	json_error_t err;

	if (jsonrpc2)
		return true;

	s = (char*) new char[128 + (sessionId ? strlen(sessionId) : 0)];
	if (false)
		sprintf(s, "{\"id\": 1, \"method\": \"mining.subscribe\", \"params\": []}");
	else if (sessionId)
		sprintf(s, "{\"id\": 1, \"method\": \"mining.subscribe\", \"params\": [\"" USER_AGENT "\", \"%s\"]}", sessionId);
	else
		sprintf(s, "{\"id\": 1, \"method\": \"mining.subscribe\", \"params\": [\"" USER_AGENT "\"]}");

	if (!sendLine(s)) { //there was a mutex but no idea why
		applog::log(LOG_ERR, "stratum_subscribe send failed");
		delete[] s;
		return false;
	}
	delete[] s;
	if (!socketFull(30)) {
		applog::log(LOG_ERR, "stratum_subscribe timed out");
		return false;
	}
	sret = receiveLine();
	if (!sret) {
		//was not log in the real
		return false;
	}
	val = json_loads(sret, 0, &err);
	free(sret);
	if (!val) {
		s=new char[220];
		sprintf(s, "JSON decode failed(%d): %s", err.line, err.text);
		applog::log(LOG_ERR, s);
		delete[] s;
		return false;
	}
	res_val = json_object_get(val, "result");
	err_val = json_object_get(val, "error");
	if (!res_val || json_is_null(res_val) || (err_val && !json_is_null(err_val))) {
		if(true){	//retry in original
			if (err_val)
				s = json_dumps(err_val, JSON_INDENT(3));
			else
				s = strdup("(unknown reason)");
			applog::log(LOG_ERR, "JSON-RPC call failed: ");
			applog::log(LOG_ERR, s);
			free(s);
		}
		json_decref(val);
		return false;
	}
	sid = getStratumSessionId(res_val);
	//here is a debug log just for log the sessionId
	if(sessionId){
		free(sessionId);
	}
	sessionId = sid ? strdup(sid) : NULL;
	nextDiff = 1.0;
	if (!getStratumExtranonce(res_val, 1)) {
		//there is no applog here
		json_decref(val);
		return false;
	}
	json_decref(val);
	return true;
	//there is one retry try in the original if it fails.
}

uint32_t poolConnecter::getblocheight()
{
	uint32_t height = 0;
	uint8_t hlen = 0, *p, *m, *pT;

	// find 0xffff tag
	p = (uint8_t*) coinbase + 32;
	m = p + 128;
	while (*p != 0xff && p < m) p++;
	while (*p == 0xff && p < m) p++;
	if (*(p-1) == 0xff && *(p-2) == 0xff) {
		p++; hlen = *p;
		pT = p;
		p++; height = ((uint16_t)(pT[0]) + ((uint16_t)(pT[1]) << 8));
		p += 2;
		switch (hlen) {
			case 3:
				height += 0x10000UL * (*p);
				break;
			case 4:
				pT = p;
				height += 0x10000UL * ((uint16_t)(pT[0]) + ((uint16_t)(pT[1]) << 8));
				break;
		}
	}
	return height;
}

bool poolConnecter::hex2bin(unsigned char *p, const char *hexstr, size_t len)
{
	char hex_byte[3];
	char *ep;

	hex_byte[2] = '\0';

	while (*hexstr && len) {
		if (!hexstr[1]) {
			applog::log(LOG_ERR, "hex2bin str truncated");
			return false;
		}
		hex_byte[0] = hexstr[0];
		hex_byte[1] = hexstr[1];
		*p = (unsigned char) strtol(hex_byte, &ep, 16);
		if (*ep) {
			applog::log(LOG_ERR, "hex2bin failed on ");
			applog::log(LOG_ERR, hex_byte);
			return false;
		}
		p++;
		hexstr += 2;
		len--;
	}

	return(!len) ? true : false;
}

bool poolConnecter::stratumNotify(json_t *params)
{
	const char *job_id, *prevhash, *coinb1, *coinb2, *version, *nbits, *ntime;
	const char *claim = NULL;
	size_t coinb1_size, coinb2_size;
	bool clean;
	int merkle_count, i, p=0;
	bool has_claim = json_array_size(params) == 10; // todo: use opt_algo
	json_t *merkle_arr;
	unsigned char **merkle;

	job_id = json_string_value(json_array_get(params, p++));
	prevhash = json_string_value(json_array_get(params, p++));
	if (has_claim) {
		claim = json_string_value(json_array_get(params, p++));
		if (!claim || strlen(claim) != 64) {
			applog::log(LOG_ERR, "Stratum notify: invalid claim parameter");
			return false;
		}
	}
	coinb1 = json_string_value(json_array_get(params, p++));
	coinb2 = json_string_value(json_array_get(params, p++));
	merkle_arr = json_array_get(params, p++);
	if (!merkle_arr || !json_is_array(merkle_arr)){
		return false;
	}
	merkle_count = (int) json_array_size(merkle_arr);
	version = json_string_value(json_array_get(params, p++));
	nbits = json_string_value(json_array_get(params, p++));
	ntime = json_string_value(json_array_get(params, p++));
	clean = json_is_true(json_array_get(params, p));

	if (!job_id || !prevhash || !coinb1 || !coinb2 || !version || !nbits || !ntime ||
		strlen(prevhash) != 64 || strlen(version) != 8 ||
		strlen(nbits) != 8 || strlen(ntime) != 8) {
		applog::log(LOG_ERR, "Stratum notify: invalid parameters");
		return false;
	}
	merkle = new unsigned char*[merkle_count/* * sizeof(char *)*/];
	for (i = 0; i < merkle_count; i++) {
		const char *s = json_string_value(json_array_get(merkle_arr, i));
		if (!s || strlen(s) != 64) {
			while (i--)
				delete[] merkle[i];
			delete[] merkle;
			applog::log(LOG_ERR, "Stratum notify: invalid Merkle branch");
			return false;
		}
		merkle[i] = new unsigned char[32];
		hex2bin(merkle[i], s, 32);
	}
	coinb1_size = strlen(coinb1) / 2;
	coinb2_size = strlen(coinb2) / 2;
	coinbaseSize = coinb1_size + xnonce1Size + xnonce2Size + coinb2_size;
	if(coinbase){
		delete[] coinbase;
	}
	coinbase = new unsigned char[coinbaseSize];
	xnonce2 = coinbase + coinb1_size + xnonce1Size;
	hex2bin(coinbase, coinb1, coinb1_size);
	memcpy(coinbase + coinb1_size, xnonce1, xnonce1Size);
	if (!jobId || strcmp(jobId, job_id))
		memset(xnonce2, 0, xnonce2Size);
	hex2bin(xnonce2 + xnonce2Size, coinb2, coinb2_size);

	free(jobId);
	jobId = strdup(job_id);
	hex2bin(prevHash, prevhash, 32);
	if (has_claim) hex2bin(jClaim, claim, 32);
	blockHeight = getblocheight();
	for (i = 0; i < merkleCount; i++)
		delete[] jMerkle[i];
	if(jMerkle)
		delete[] jMerkle;
	jMerkle = merkle;
	merkleCount = merkle_count;

	hex2bin(jVersion, version, 4);
	hex2bin(jNbits, nbits, 4);
	hex2bin(jNtime, ntime, 4);
	jClean = clean;

	jDiff = nextDiff;
	return true;
}

bool poolConnecter::stratumStats(json_t *id, json_t *params)
{
	char *s;
	json_t *val, *valTmp;
	bool ret=false;

	if (!id || json_is_null(id))
		return false;

	val = json_object();
	json_object_set(val, "id", id);

	if (!ret) {
		json_t *valT = json_object();
		json_object_set_new(valT, "code", json_integer(1));
		json_object_set_new(valT, "message", json_string("disabled"));
		json_object_set_new(val, "error", valT);
	} else {
		json_object_set_new(val, "error", json_null());
	}

	s = json_dumps(val, 0);
	ret = sendLine(s);
	json_decref(val);
	free(s);

	return ret;
}

int poolConnecter::shareResult(int result, const char *reason)
{
	char suppl[32] = { 0 };
	char s[345];
	int i;
	result ? acceptedCount++ : rejectedCount++;
	//global_hashrate = (uint64_t) hashrate;
	sprintf(suppl, "%.2f%%", 100. * acceptedCount / (acceptedCount + rejectedCount));
	sprintf(s, "accepted: %lu/%lu (%s), %s kH/s %s", acceptedCount, acceptedCount + rejectedCount, suppl, "(some Hashrateinfo)");
	applog::log(LOG_NOTICE, s);
	if (reason) {
		applog::log(LOG_WARNING, "reject reason:");
		applog::log(LOG_WARNING, reason);
		if (0 && strncmp(reason, "low difficulty share", 20) == 0) {
			applog::log(LOG_WARNING, "factor reduced to : (2/3)-hardcoded");
			return 0;
		}
	}
	return 1;
}

bool poolConnecter::handleStratumResponse(char* buf)
{
	json_t *val, *err_val, *res_val, *id_val;
	json_error_t err;
	bool valid = false;

	val = json_loads(buf, 0, &err);
	if (!val) {
		char *s=new char[220];
		sprintf(s, "JSON decode failed(%d): %s", err.line, err.text);
		applog::log(LOG_INFO, s);
		delete[] s;
		return false;
	}

	res_val = json_object_get(val, "result");
	err_val = json_object_get(val, "error");
	id_val = json_object_get(val, "id");

	if (!id_val || json_is_null(id_val)){
		json_decref(val);
		return false;
	}
	if(jsonrpc2) {
		if (!res_val && !err_val){
			json_decref(val);
			return false;
		}
		json_t *status = json_object_get(res_val, "status");
		if(status) {
			const char *s = json_string_value(status);
			valid = !strcmp(s, "OK") && json_is_null(err_val);
		} else {
			valid = json_is_null(err_val);
		}
		shareResult(valid, err_val ? json_string_value(err_val) : NULL);
	}
	else {
		if (!res_val || json_integer_value(id_val) < 4){
			json_decref(val);
			return false;
		}
		valid = json_is_true(res_val);
		shareResult(valid, err_val ? json_string_value(json_array_get(err_val, 1)) : NULL);
	}
	json_decref(val);
	return true;
}

bool poolConnecter::rpc2LoginDecode(const json_t *val)
{
	const char *id;
	const char *s;

	json_t *res = json_object_get(val, "result");
	if(!res) {
		applog::log(LOG_ERR, "JSON invalid result");
		return false;
	}

	json_t *tmp;
	tmp = json_object_get(res, "id");
	if(!tmp) {
		applog::log(LOG_ERR, "JSON inval id");
		return false;
	}
	id = json_string_value(tmp);
	if(!id) {
		applog::log(LOG_ERR, "JSON id is not a string");
		return false;
	}

	memcpy(&rpc2Id, id, 64);

	tmp = json_object_get(res, "status");
	if(!tmp) {
		applog::log(LOG_ERR, "JSON inval status");
		return false;
	}
	s = json_string_value(tmp);
	if(!s) {
		applog::log(LOG_ERR, "JSON status is not a string");
		return false;
	}
	if(strcmp(s, "OK")) {
		applog::log(LOG_ERR, "JSON returned status: ");
		applog::log(LOG_ERR, s);
		return false;
	}

	return true;
}

bool poolConnecter::rpc2JobDecode(const json_t *params)
{
	if (!jsonrpc2) {
		applog::log(LOG_ERR, "Tried to decode job without JSON-RPC 2.0");
		return false;
	}
	json_t *tmp;
	tmp = json_object_get(params, "job_id");
	if (!tmp) {
		applog::log(LOG_ERR, "JSON invalid job id");
		return false;
	}
	const char *job_id = json_string_value(tmp);
	tmp = json_object_get(params, "blob");
	if (!tmp) {
		applog::log(LOG_ERR, "JSON invalid blob");
		return false;
	}
	const char *hexblob = json_string_value(tmp);
	size_t blobLen = strlen(hexblob);
	if (blobLen % 2 != 0 || ((blobLen / 2) < 40 && blobLen != 0) || (blobLen / 2) > 128) {
		applog::log(LOG_ERR, "JSON invalid blob length");
		return false;
	}
	if (blobLen != 0) {
		uint32_t target = 0;
		unsigned char *blob = new unsigned char[blobLen / 2];
		if (!hex2bin(blob, hexblob, blobLen / 2)) {
			applog::log(LOG_ERR, "JSON invalid blob");
			return false;
		}
		rpc2Bloblen = blobLen / 2;
		if (rpc2Blob) delete[] rpc2Blob;
		rpc2Blob = new char[rpc2Bloblen];
		if (!rpc2Blob)  {
			applog::log(LOG_ERR, "RPC2 Out of Memory!");
			return false;
		}
		memcpy(rpc2Blob, blob, blobLen / 2);
		delete[] blob;

		const char *hexstr;
		tmp = json_object_get(params, "target");
		if (!tmp) {
			applog::log(LOG_ERR, "JSON key 'target' not found");
			return false;
		}
		hexstr = json_string_value(tmp);
		if (!hexstr) {
			applog::log(LOG_ERR, "JSON key 'target' is not a string");
			return false;
		}
		if (!hex2bin((unsigned char*) &target, hexstr, 4))
			return false;
		
		if(rpc2Target != target) {
			double hashrate = 0.0;
			double difficulty = (((double) 0xffffffff) / target);
			//if (!opt_quiet) {
				// xmr pool diff can change a lot...
			//	applog(LOG_WARNING, "Stratum difficulty set to %g", difficulty);
			//}
			stratumDiff = difficulty;
			rpc2Target = target;
		}

		if (jobId) free(jobId);
		jobId = strdup(job_id);
	}
	return true;
}

bool poolConnecter::handleStratumMethod(const char* s)
{
	json_t *val, *id, *params, *valT;
	json_error_t err;
	const char *method;
	bool ret = false;

	val = json_loads(s, 0, &err);
	if (!val) {
		char* s=new char[220];
		sprintf(s, "JSON decode failed(%d): %s", err.line, err.text);
		applog::log(LOG_ERR, s);
		delete[] s;
		return false;
	}
	method = json_string_value(json_object_get(val, "method"));
	if (!method){
		json_decref(val);
		return false;
	}
	params = json_object_get(val, "params");
	id = json_object_get(val, "id");

	if (jsonrpc2) {
		if (!strcasecmp(method, "job")) {
			ret = rpc2JobDecode(params);
			json_decref(val);
			return ret;
		}
		return false;
	}

	if (!strcasecmp(method, "mining.notify")) {
		ret = stratumNotify(params);
		json_decref(val);
		return ret;
	}
	else if (!strcasecmp(method, "mining.ping")) { // cgminer 4.7.1+
		//if (opt_debug) applog(LOG_DEBUG, "Pool ping");
		char buf[64];
		if (!id || json_is_null(id))
			return false;
		sprintf(buf, "{\"id\":%d,\"result\":\"pong\",\"error\":null}",
			(int) json_integer_value(id));
		json_decref(val);
		return sendLine(buf);
	}
	else if (!strcasecmp(method, "mining.set_difficulty")) {
		double diff;
		diff = json_number_value(json_array_get(params, 0));
		if (diff == 0)
			return false;
		nextDiff = diff;
		json_decref(val);
		return true;
	}
	else if (!strcasecmp(method, "mining.set_extranonce")) {
		ret = getStratumExtranonce(params, 0);
		json_decref(val);
		return ret;
	}
	else if (!strcasecmp(method, "client.reconnect")) {
		json_t *port_val;
		char *urlT;
		const char *host;
		int port;
		host = json_string_value(json_array_get(params, 0));
		port_val = json_array_get(params, 1);
		if (json_is_string(port_val))
			port = atoi(json_string_value(port_val));
		else
			port = (int) json_integer_value(port_val);
		if (!host || !port){
			json_decref(val);
			return false;
		}
		urlT = new char[32 + strlen(host)];
		sprintf(urlT, "stratum+tcp://%s:%d", host, port);
		json_decref(val);
		if (false) {//no-redirect option in the original
			applog::log(LOG_INFO, "Ignoring request to reconnect to ");
			applog::log(LOG_INFO, url);
			delete[] urlT;
			return true;
		}
		applog::log(LOG_NOTICE, "Server requested reconnection to ");
		applog::log(LOG_NOTICE, url);
		delete[] url;
		url = urlT;
		if (curl) {
			curl_easy_cleanup(curl);
			curl = NULL;
			sockBuf[0] = '\0';
		}
		return true;
	}
	else if (!strcasecmp(method, "client.get_algo")) {
		// will prevent wrong algo parameters on a pool, will be used as test on rejects
		//if (!opt_quiet) applog(LOG_NOTICE, "Pool asked your algo parameter");
		char algo[64] = { 0 };
		char *s;
		ret = true;
		if (!id || json_is_null(id))
			return false;

		//change if more than one algo is used
		snprintf(algo, sizeof(algo), "%s", "scrypt");

		valT = json_object();
		json_object_set(valT, "id", id);
		json_object_set_new(valT, "error", json_null());
		json_object_set_new(valT, "result", json_string(algo));
		s = json_dumps(valT, 0);
		ret = sendLine(s);
		json_decref(valT);
		json_decref(val);
		free(s);
		return ret;
	}
	else if (!strcasecmp(method, "client.get_stats")) {
		// optional to fill device benchmarks
		ret = stratumStats(id, params);
		json_decref(val);
		return ret; //send error back
	}
	else if (!strcasecmp(method, "client.get_version")) {
		char *s;
		if (!id || json_is_null(id))
			return false;
		valT = json_object();
		json_object_set(valT, "id", id);
		json_object_set_new(valT, "error", json_null());
		json_object_set_new(valT, "result", json_string(USER_AGENT));
		s = json_dumps(valT, 0);
		ret = sendLine(s);
		json_decref(valT);
		json_decref(val);
		free(s);
		return ret;
	}
	else if (!strcasecmp(method, "client.show_message")) {
		char *s;
		valT = json_array_get(params, 0);
		if (valT) {
			applog::log(LOG_NOTICE, "MESSAGE FROM SERVER:");
			applog::log(LOG_NOTICE, json_string_value(valT));
		}
		if (!id || json_is_null(id)){
			json_decref(val);
			return true;
		}
		valT = json_object();
		json_object_set(valT, "id", id);
		json_object_set_new(valT, "error", json_null());
		json_object_set_new(valT, "result", json_true());
		s = json_dumps(valT, 0);
		ret = sendLine(s);
		json_decref(valT);
		json_decref(val);
		free(s);
		return ret;
	}
	//unknown/not implemented method:
	else{
		char *s;
		if (!id || json_is_null(id))
			return ret;
		valT = json_object();
		json_object_set(valT, "id", id);
		json_object_set_new(valT, "result", json_false());
		json_t *valTmp = json_object();
		json_object_set_new(valTmp, "code", json_integer(38));
		json_object_set_new(valTmp, "message", json_string("unknown method"));
		json_object_set_new(valT, "error", valTmp);
		s = json_dumps(valT, 0);
		ret = sendLine(s);
		json_decref(valT);
		json_decref(val);
		free(s);
		return ret;
	}
}

bool poolConnecter::stratumAuthorize()
{
	json_t *val = NULL, *res_val, *err_val;
	char *s, *sret;
	int req_id = 0;
	json_error_t err;
	size_t userLength=0,passLength=0;
	if(user)
		while(user[userLength] != '\0')
			userLength++;
	if(pass)
		while(pass[passLength] != '\0')
			passLength++;

	if (jsonrpc2) {
		s = new char[300 + userLength + passLength];
		sprintf(s, "{\"method\": \"login\", \"params\": {\"login\": \"%s\", \"pass\": \"%s\", \"agent\": \"%s\"}, \"id\": 1}", user, pass, USER_AGENT);
	} else {
		s = new char[80 + userLength + passLength];
		sprintf(s, "{\"id\": 2, \"method\": \"mining.authorize\", \"params\": [\"%s\", \"%s\"]}", user, pass);
	}
	if (!sendLine(s)) { //there was a mutex but no idea why
		delete[] s;
		return false;
	}
	delete[] s;
	while (1) {
		sret = receiveLine();
		if (!sret)
			return false;
		if (!handleStratumMethod(sret))
			break;
		free(sret);
	}
	val = json_loads(sret, 0, &err);
	free(sret);
	if (!val) {
		s=new char[220];
		sprintf(s, "JSON decode failed(%d): %s", err.line, err.text);
		applog::log(LOG_ERR, s);
		delete[] s;
		return false;
	}
	res_val = json_object_get(val, "result");
	err_val = json_object_get(val, "error");
	req_id = (int) json_integer_value(json_object_get(val, "id"));

	if (req_id == 2 && (!res_val || json_is_false(res_val) || (err_val && !json_is_null(err_val)))) {
		applog::log(LOG_ERR, "Stratum authentication failed");
		json_decref(val);
		return false;
	}
	if (jsonrpc2) {
		rpc2LoginDecode(val);
		json_t *job_val = json_object_get(res_val, "job");
		if(job_val)
			rpc2JobDecode(job_val);
	}

	s = new char[80];
	sprintf(s, "{\"id\": 3, \"method\": \"mining.extranonce.subscribe\", \"params\": []}");
	if (!sendLine(s)){
		delete[] s;
		json_decref(val);
		return true;
	}
	delete[] s;
	if (!socketFull(3)) {
		json_decref(val);
		//if (opt_debug)
		//	applog(LOG_DEBUG, "stratum extranonce subscribe timed out");
		return true;
	}
	sret = receiveLine();
	if (sret) {
		json_t *extra = json_loads(sret, 0, &err);
		if (!extra) {
			s=new char[220];
			sprintf(s, "JSON decode failed(%d): %s", err.line, err.text);
			applog::log(LOG_WARNING, s);
			delete[] s;
		} else {
			if (json_integer_value(json_object_get(extra, "id")) != 3) {
				// we receive a standard method if extranonce is ignored
				if (!handleStratumMethod(sret)){
					applog::log(LOG_WARNING, "Stratum answer id is not correct!");
				}
			}
			json_decref(extra);
		}
		free(sret);
	}
	json_decref(val);
	return true;
}

void poolConnecter::prepareWorkDatas()
{
	if(jsonrpc2){
		if(!rpc2Blob){
			applog::log(LOG_ERR, "...terminating workio thread");
			return;
		}
		memcpy(wData, rpc2Blob, rpc2Bloblen);
		memset(wTarget, 0xff, sizeof(wTarget));
		wTarget[7]=rpc2Target;
		if(wJobId)
			free(wJobId);
		wJobId=strdup(jobId);
	}
	else{
		unsigned char merkle_root[64] = { 0 };
		free(wJobId);
		wJobId=strdup(jobId);
		//xnonce2..
		sha256Abstract::sha256d(merkle_root,coinbase,(int)coinbaseSize);
		for (int i = 0; i < merkleCount; i++) {
			memcpy(merkle_root + 32, jMerkle[i], 32);
			sha256Abstract::sha256d(merkle_root, merkle_root, 64);
		}
		//xnonce2 again
		memset(wData,0,128);
		wData[0] = le32dec(jVersion);
		for (int i = 0; i < 8; i++){
			wData[1 + i] = le32dec((uint32_t *) prevHash + i);
			wData[9 + i] = sha256Abstract::be32dec((uint32_t *) merkle_root + i);
		}
		wData[17] = le32dec(jNtime);
		wData[18] = le32dec(jNbits);
		// required ?
		wData[20] = 0x80000000;
		wData[31] = 0x00000280;

		double diffTmp=jDiff/65536.0;
		int k;
		for (k = 6; k > 0 && diffTmp > 1.0; k--)
			diffTmp /= 4294967296.0;
		uint64_t m=(uint64_t)(4294901760.0 / diffTmp);
		if (m == 0 && k == 6)
			memset(wTarget, 0xff, 32);
		else {
			memset(wTarget, 0, 32);
			wTarget[k] = (uint32_t)m;
			wTarget[k + 1] = (uint32_t)(m >> 32);
		}
	}
}

void* poolConnecter::poolMainMethod(void *poolThreadPar)
{
	poolThreadParam *tParam=(poolThreadParam*)poolThreadPar;
	poolConnecter *poolConnObject=tParam->poolConnObjRef;
	char *s;
	int failures;
	while(1){
		failures=0;
		while (!poolConnObject->curl) {
			if (!poolConnObject->stratumConnect() || !poolConnObject->stratumSubscribe() || !poolConnObject->stratumAuthorize()) {
				if (poolConnObject->curl) {
					curl_easy_cleanup(poolConnObject->curl);
					poolConnObject->curl = NULL;
					poolConnObject->sockBuf[0] = '\0';
				}
				if (poolConnObject->retryCount >= 0 && ++failures > poolConnObject->retryCount) {
					applog::log(LOG_ERR, "...terminating workio thread");
					//tq_push(thr_info[work_thr_id].q, NULL);
					return NULL;
				}
				applog::log(LOG_ERR, "...retry after 10 seconds");
				sleep(10);
			}
		}

		//strcmp fails with exception until I added mystrcmp...which i dont even use
		if(strcmp(poolConnObject->wJobId,poolConnObject->jobId)) {
			poolConnObject->tBufL=2+5+strlen(poolConnObject->wJobId)+7+strlen(poolConnObject->jobId);
			poolConnObject->tBuf=new char[poolConnObject->tBufL];
			char *justTest=poolConnObject->tBuf;
			justTest[0]=0;justTest[1]=4;
			justTest[2]='o';justTest[3]='l';justTest[4]='d';justTest[5]=':'; justTest[6]=' ';
			int ind;
			for(ind=7;ind<strlen(poolConnObject->wJobId)+7;ind++){
				justTest[ind]=poolConnObject->wJobId[ind-7];
			}
			justTest[ind++]=','; justTest[ind++]=' '; justTest[ind++]='n'; justTest[ind++]='e';
			justTest[ind++]='w'; justTest[ind++]=':'; justTest[ind++]=' ';
			int indi;
			for(indi=ind;indi<strlen(poolConnObject->jobId)+ind;indi++){
				justTest[indi]=poolConnObject->jobId[indi-ind];
			}
			//if works i can change it to datas to send
			delete[] justTest;
			poolConnObject->prepareWorkDatas();
			poolConnObject->tBufL=2+5+80+32+8;
			poolConnObject->tBuf=new char[poolConnObject->tBufL];
			justTest=poolConnObject->tBuf;
			justTest[0]=0;justTest[1]=4;
			for(ind=2;ind<strlen(poolConnObject->wJobId)+2;ind++){
				justTest[ind]=poolConnObject->wJobId[ind-2];
			}
			for(int i=0;i<20;i++){
				char num1=poolConnObject->wData[i]>>24;
				char num2=poolConnObject->wData[i]>>16;
				char num3=poolConnObject->wData[i]>>8;
				char num4=poolConnObject->wData[i];
				justTest[ind++]=num1;justTest[ind++]=num2;
				justTest[ind++]=num3;justTest[ind++]=num4;
			}
			for(int i=0;i<8;i++){
				char tar1=poolConnObject->wTarget[i]>>24;
				char tar2=poolConnObject->wTarget[i]>>16;
				char tar3=poolConnObject->wTarget[i]>>8;
				char tar4=poolConnObject->wTarget[i];
				justTest[ind++]=tar1;justTest[ind++]=tar2;
				justTest[ind++]=tar3;justTest[ind++]=tar4;
			}
			justTest[ind++]=0;justTest[ind++]=0;
			justTest[ind++]=0;justTest[ind++]=0;
			justTest[ind++]=210;justTest[ind++]=255;
			justTest[ind++]=255;justTest[ind++]=255;
			tParam->notifyFunc(poolConnObject);
			delete[] justTest;
		}

		if (!poolConnObject->socketFull(300)) {
			applog::log(LOG_ERR, "Stratum connection timeout");
			s = NULL;
		} else
			s = poolConnObject->receiveLine();
		if (!s) {
			if (poolConnObject->curl) {
				curl_easy_cleanup(poolConnObject->curl);
				poolConnObject->curl = NULL;
				poolConnObject->sockBuf[0] = '\0';
			}
			applog::log(LOG_ERR, "Stratum connection interrupted");
			continue;
		}
		if (!poolConnObject->handleStratumMethod(s)){
			poolConnObject->handleStratumResponse(s);
		}
		free(s);
	}
	return NULL;
}