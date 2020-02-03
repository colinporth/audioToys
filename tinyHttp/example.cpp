//{{{
/*-
 * Copyright 2012 Matthew Endsley
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


/*
Compiling example:
$ g++ -o example example.cpp
*/
//}}}
//{{{  includes
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <winsock2.h>
#include <WS2tcpip.h>
//#include <sys/socket.h>
//#include <netdb.h>
//#include <unistd.h>

#include <string>
#include <vector>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#include "http.h"
#include "header.h"
#include "chunk.h"

using namespace std;
//}}}

//{{{
// return a socket connected to a hostname, or -1
int connectsocket (const char* host, int port) {
// return a socket connected to a hostname, or -1

	// win32 find host ipAddress
	struct addrinfo hints;
	memset (&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;       // IPv4
	hints.ai_protocol = IPPROTO_TCP; // TCP
	hints.ai_socktype = SOCK_STREAM; // TCP so its SOCK_STREAM
	struct addrinfo* targetAddressInfo = NULL;
	unsigned long getAddrRes = getaddrinfo (host, NULL, &hints, &targetAddressInfo);
	if (getAddrRes != 0 || targetAddressInfo == NULL)
		return -1;

	// form sockAddr
	struct sockaddr_in sockAddr;
	sockAddr.sin_addr = ((struct sockaddr_in*)targetAddressInfo->ai_addr)->sin_addr;
	sockAddr.sin_family = AF_INET; // IPv4
	sockAddr.sin_port = htons (port); // HTTP Port: 80

	// free targetAddressInfo from getaddrinfo
	freeaddrinfo (targetAddressInfo);

	// win32 create webSocket
	auto mSocket = (unsigned int)socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (mSocket < 0)
		return -2;

	// win32 connect webSocket
	if (connect (mSocket, (SOCKADDR*)&sockAddr, sizeof(sockAddr))) {
		closesocket (mSocket);
		mSocket = -1;
		return -3;
		}

	return mSocket;
}
//}}}

//{{{
// Response data/funcs
struct HttpResponse {
	std::vector<char> body;
	int code;
	};
//}}}

//{{{
static void* response_realloc (void* opaque, void* ptr, int size) {
	return realloc (ptr, size);
	}
//}}}
//{{{
static void response_body (void* opaque, const char* data, int size) {
	HttpResponse* response = (HttpResponse*)opaque;
	response->body.insert(response->body.end(), data, data + size);
	}
//}}}
//{{{
static void response_header (void* opaque, const char* ckey, int nkey, const char* cvalue, int nvalue) {
	}
//}}}
//{{{
static void response_code (void* opaque, int code) {
	HttpResponse* response = (HttpResponse*)opaque;
	response->code = code;
	}
//}}}

static const http_funcs responseFuncs = {
	response_realloc,
	response_body,
	response_header,
	response_code,
	};

//{{{
int main() {

	WSADATA wsaData;
	if (WSAStartup (MAKEWORD(2,2), &wsaData))
		exit (0);

	int conn = connectsocket("nothings.org", 80);
		if (conn < 0) {
			fprintf(stderr, "Failed to connect socket\n");
			return -1;
			}

	const char request[] = "GET / HTTP/1.0\r\nContent-Length: 0\r\n\r\n";
	int len = send (conn, request, sizeof(request) - 1, 0);
	if (len != sizeof(request) - 1) {
		fprintf (stderr, "Failed to send request\n");
		closesocket (conn);
		return -1;
		}

	HttpResponse response;
	response.code = 0;

	http_roundtripper rt;
	http_init (&rt, responseFuncs, &response);

	bool needmore = true;
	char buffer[1024];
	while (needmore) {
		const char* data = buffer;
		int ndata = recv (conn, buffer, sizeof(buffer), 0);
		if (ndata <= 0) {
			fprintf(stderr, "Error receiving data\n");
			http_free(&rt);
			closesocket(conn);
			return -1;
			}

		while (needmore && ndata) {
			int read;
			needmore = http_data (&rt, data, ndata, &read);
			ndata -= read;
			data += read;
			}
		}

	if (http_iserror (&rt)) {
			fprintf (stderr, "Error parsing data\n");
			http_free (&rt);
			closesocket (conn);
			return -1;
	}

	http_free(&rt);
	closesocket (conn);

	printf ("Response: %d\n", response.code);
	if (!response.body.empty()) {
		printf("%s\n", &response.body[0]);
		}

	return 0;
	}
//}}}
