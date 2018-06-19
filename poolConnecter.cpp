#include"poolConnecter.hpp"

poolConnecter::poolConnecter()
{
	retryCount=-1;
	url=new char[50];
	strcpy(url,"stratum+tcp://killallasics.moneroworld.com:5555");
	user=new char[100];
	strcpy(user,"9v4vTVwqZzfjCFyPi7b9Uv1hHntJxycC4XvRyEscqwtq8aycw5xGpTxFyasurgf2KRBfbdAJY4AVcemL1JCegXU4EZfMtaz");
}

poolConnecter::poolConnecter(char* stratumUrl, char *stratUser, char *stratPass)
{
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
	curl_easy_setopt(curl, CURLOPT_OPENSOCKETDATA, sock);
	curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1);
	rc = curl_easy_perform(curl);
	if (rc) {
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
			return sret;
		}
	}

	buflen = (ssize_t) strlen(sockBuf);
	tok = strtok(sockBuf, "\n");
	if (!tok) {
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
		return false;
	}
	xn2Size = (int) json_integer_value(json_array_get(val, pndx+1));
	if (!xn2Size) {
		return false;
	}
	if (xn2Size < 2 || xn2Size > 16) {
		return false;
	}
	if (xnonce1)
		free(xnonce1);
	xnonce1Size = strlen(xn1) / 2;
	xnonce1 = (unsigned char*) calloc(1, xnonce1Size);
	if (!xnonce1) {
		return false;
	}
	hex2bin(xnonce1, xn1, xnonce1Size);

	xnonce2Size = xn2Size;
	return true;
}

bool poolConnecter::stratumSubscribe()
{
	char *s, *sret = NULL;
	bool ret = false;
	const char *sid;
	json_t *val = NULL, *res_val, *err_val;
	json_error_t err;
	s = (char*) new char[128 + (sessionId ? strlen(sessionId) : 0)];
	if (false)
		sprintf(s, "{\"id\": 1, \"method\": \"mining.subscribe\", \"params\": []}");
	else if (sessionId)
		sprintf(s, "{\"id\": 1, \"method\": \"mining.subscribe\", \"params\": [\"" USER_AGENT "\", \"%s\"]}", sessionId);
	else
		sprintf(s, "{\"id\": 1, \"method\": \"mining.subscribe\", \"params\": [\"" USER_AGENT "\"]}");

	if (!sendLine(s)) { //there was a mutex but no idea why
		delete[] s;
		return false;
	}
	delete[] s;
	if (!socketFull(30)) {
		return false;
	}
	sret = receiveLine();
	if (!sret) {
		return false;
	}
	val = json_loads(sret, 0, &err);
	free(sret);
	if (!val) {
		return false;
	}
	res_val = json_object_get(val, "result");
	err_val = json_object_get(val, "error");
	json_decref(val);
	if (!res_val || json_is_null(res_val) || (err_val && !json_is_null(err_val))) {
		return false;
	}
	sid = getStratumSessionId(res_val);
	if(sessionId){
		free(sessionId);
	}
	sessionId = sid ? strdup(sid) : NULL;
	nextDiff = 1.0;
	if (!getStratumExtranonce(res_val, 1)) {
		return false;
	}
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
			//applog(LOG_ERR, "hex2bin str truncated");
			return false;
		}
		hex_byte[0] = hexstr[0];
		hex_byte[1] = hexstr[1];
		*p = (unsigned char) strtol(hex_byte, &ep, 16);
		if (*ep) {
			//applog(LOG_ERR, "hex2bin failed on '%s'", hex_byte);
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
			//applog(LOG_ERR, "Stratum notify: invalid claim parameter");
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
		//applog(LOG_ERR, "Stratum notify: invalid parameters");
		return false;
	}
	merkle = new unsigned char*[merkle_count * sizeof(char *)];
	for (i = 0; i < merkle_count; i++) {
		const char *s = json_string_value(json_array_get(merkle_arr, i));
		if (!s || strlen(s) != 64) {
			while (i--)
				delete[] merkle[i];
			delete[] merkle;
			//applog(LOG_ERR, "Stratum notify: invalid Merkle branch");
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

bool poolConnecter::handleStratumResponse(char* buf)
{
	json_t *val, *err_val, *res_val, *id_val;
	json_error_t err;
	bool valid = false;

	val = json_loads(buf, 0, &err);
	if (!val) {
		//applog(LOG_INFO, "JSON decode failed(%d): %s", err.line, err.text);
		return false;
	}

	res_val = json_object_get(val, "result");
	err_val = json_object_get(val, "error");
	id_val = json_object_get(val, "id");
	json_decref(val);

	if (!id_val || json_is_null(id_val))
		return false;
	if (!res_val || json_integer_value(id_val) < 4)
		return false;
	valid = json_is_true(res_val);
	//share_result(valid, NULL, err_val ? json_string_value(json_array_get(err_val, 1)) : NULL);
	//log some reject/share info
	return true;
}

bool poolConnecter::handleStratumMessage(const char* s)
{
	json_t *val, *id, *params, *valT;
	json_error_t err;
	const char *method;
	bool ret = false;

	val = json_loads(s, 0, &err);
	if (!val) {
		return false;
	}
	method = json_string_value(json_object_get(val, "method"));
	if (!method){
		json_decref(val);
		return false;
	}
	params = json_object_get(val, "params");
	id = json_object_get(val, "id");
	json_decref(val);
	if (!strcasecmp(method, "mining.notify")) {
		return(stratumNotify(params));
	}
	else if (!strcasecmp(method, "mining.ping")) { // cgminer 4.7.1+
		//if (opt_debug) applog(LOG_DEBUG, "Pool ping");
		char buf[64];
		if (!id || json_is_null(id))
			return false;
		sprintf(buf, "{\"id\":%d,\"result\":\"pong\",\"error\":null}",
			(int) json_integer_value(id));
		return(sendLine(buf));
	}
	else if (!strcasecmp(method, "mining.set_difficulty")) {
		double diff;
		diff = json_number_value(json_array_get(params, 0));
		if (diff == 0)
			return false;
		nextDiff = diff;
		return true;
	}
	else if (!strcasecmp(method, "mining.set_extranonce")) {
		return(getStratumExtranonce(params, 0));
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
		if (!host || !port)
			return false;
		urlT = new char[32 + strlen(host)];
		sprintf(urlT, "stratum+tcp://%s:%d", host, port);
		if (false) {//no-redirect option in the original
			//applog(LOG_INFO, "Ignoring request to reconnect to %s", url);
			delete[] urlT;
			return true;
		}
		//applog(LOG_NOTICE, "Server requested reconnection to %s", url);
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
		free(s);
		return ret;
	}
	else if (!strcasecmp(method, "client.get_stats")) {
		// optional to fill device benchmarks
		return stratumStats(id, params); //send error back
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
		free(s);
		return ret;
	}
	else if (!strcasecmp(method, "client.show_message")) {
		char *s;
		valT = json_array_get(params, 0);
		//if (valT)//TODO: reason for this part
		//	applog(LOG_NOTICE, "MESSAGE FROM SERVER: %s", json_string_value(valT));
		if (!id || json_is_null(id))
			return true;
		valT = json_object();
		json_object_set(valT, "id", id);
		json_object_set_new(valT, "error", json_null());
		json_object_set_new(valT, "result", json_true());
		s = json_dumps(valT, 0);
		ret = sendLine(s);
		json_decref(valT);
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
	while(user[userLength] != '\0')
		userLength++;
	while(pass[passLength] != '\0')
		passLength++;
	s = new char[80 + userLength + passLength];
	sprintf(s, "{\"id\": 2, \"method\": \"mining.authorize\", \"params\": [\"%s\", \"%s\"]}", user, pass);
	if (!sendLine(s)) { //there was a mutex but no idea why
		delete[] s;
		return false;
	}
	delete[] s;
	while (1) {
		sret = receiveLine();
		if (!sret)
			return false;
		if (!handleStratumMessage(sret))
			break;
		free(sret);
	}
	val = json_loads(sret, 0, &err);
	free(sret);
	if (!val) {
		//applog(LOG_ERR, "JSON decode failed(%d): %s", err.line, err.text);
		return false;
	}
	res_val = json_object_get(val, "result");
	err_val = json_object_get(val, "error");
	req_id = (int) json_integer_value(json_object_get(val, "id"));
	if (val)
		json_decref(val);

	if (req_id == 2 && (!res_val || json_is_false(res_val) || (err_val && !json_is_null(err_val)))) {
		//applog(LOG_ERR, "Stratum authentication failed");
		return false;
	}
	s = new char[80];
	sprintf(s, "{\"id\": 3, \"method\": \"mining.extranonce.subscribe\", \"params\": []}");
	if (!sendLine(s)){
		delete[] s;
		return true;
	}
	delete[] s;
	if (!socketFull(3)) {
		//if (opt_debug)
		//	applog(LOG_DEBUG, "stratum extranonce subscribe timed out");
		return true;
	}
	sret = receiveLine();
	if (sret) {
		json_t *extra = json_loads(sret, 0, &err);
		if (!extra) {
			//applog(LOG_WARNING, "JSON decode failed(%d): %s", err.line, err.text);
		} else {
			if (json_integer_value(json_object_get(extra, "id")) != 3) {
				// we receive a standard method if extranonce is ignored
				if (!handleStratumMessage(sret)){
					//applog(LOG_WARNING, "Stratum answer id is not correct!");
				}
			}
			json_decref(extra);
		}
		free(sret);
	}
	return true;
}

void* poolConnecter::poolMainMethod()
{
	//I did not copy the jsonRpc2 code
	char *s;
	int failures;
	while(1){
		failures=0;
		while (!curl) {
			if (!stratumConnect() || !stratumSubscribe() || !stratumAuthorize()) {
				if (curl) {
					curl_easy_cleanup(curl);
					curl = NULL;
					sockBuf[0] = '\0';
				}
				if (retryCount >= 0 && ++failures > retryCount) {
					//applog(LOG_ERR, "...terminating workio thread");
					//tq_push(thr_info[work_thr_id].q, NULL);
					return NULL;
				}
				//applog(LOG_ERR, "...retry after %d seconds", opt_fail_pause);
				sleep(10);
			}
		}

		//i skipped a big if cuz i had no idea what it does

		if (!socketFull(300)) {
			//applog(LOG_ERR, "Stratum connection timeout");
			s = NULL;
		} else
			s = receiveLine();
		if (!s) {
			if (curl) {
				curl_easy_cleanup(curl);
				curl = NULL;
				sockBuf[0] = '\0';
			}
			//applog(LOG_ERR, "Stratum connection interrupted");
			continue;
		}
		if (!handleStratumMessage(s)){
			handleStratumResponse(s);
		}
		free(s);
	}
	return NULL;
}