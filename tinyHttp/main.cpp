// minimal winSock http get parser, based on tinyHttp
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
//}}}
//{{{  includes
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <winsock2.h>
#include <WS2tcpip.h>

#include <string>
#include <vector>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#include "tinyHttp.h"

#include "../../shared/utils/cLog.h"

using namespace std;
//}}}

//{{{
// return a socket connected to a hostname, or -1
int connectSocket (const char* host, int port) {
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
void* responseRealloc (void* opaque, void* ptr, int size) {
	return realloc (ptr, size);
	}
//}}}
//{{{
void responseBody (void* opaque, const char* data, int size) {
	HttpResponse* response = (HttpResponse*)opaque;
	response->body.insert(response->body.end(), data, data + size);
	}
//}}}
//{{{
void responseHeader (void* opaque, const char* ckey, int nkey, const char* cvalue, int nvalue) {
	}
//}}}
//{{{
void responseCode (void* opaque, int code) {
	HttpResponse* response = (HttpResponse*)opaque;
	response->code = code;
	}
//}}}

//{{{
const cTinyHttp::http_funcs responseFuncs = {
	responseRealloc,
	responseBody,
	responseHeader,
	responseCode,
	};
//}}}

int main() {

	cLog::init (LOGINFO, false, "",  "tinyHttp");

	WSADATA wsaData;
	if (WSAStartup (MAKEWORD(2,2), &wsaData))
		exit (0);

	string hostStr = "nothings.org"; string pathStr = "";
	//string hostStr = "stream.wqxr.org"; string pathStr = "js-stream.aac";

	int socket = connectSocket (hostStr.c_str(), 80);
	if (socket < 0) {
		//{{{  error
		cLog::log (LOGERROR, "Failed to connect socket");
		return -1;
		}
		//}}}

	string requestStr = "GET /" + pathStr + " HTTP/1.1\r\nHost: " + hostStr + "\r\n\r\n";
	if (send (socket, requestStr.c_str(), (int)requestStr.size(), 0) != (int)requestStr.size()) {
		//{{{  error
		cLog::log (LOGERROR, "request send failed");
		closesocket (socket);
		return -1;
		}
		//}}}

	HttpResponse response;
	response.code = 0;
	cTinyHttp http (responseFuncs, &response);

	bool ok = true;
	while (ok) {
		char buffer[1024];
		int bytesReceived = recv (socket, buffer, sizeof(buffer), 0);
		if (bytesReceived <= 0) {
			cLog::log (LOGERROR, "Error receiving data");
			closesocket (socket);
			return -1;
			}

		const char* data = buffer;
		while (ok && bytesReceived) {
			int bytesParsed;
			ok = http.parseData (data, bytesReceived, &bytesParsed);
			cLog::log (LOGINFO, "%d %d %d", ok, bytesReceived, bytesParsed);
			bytesReceived -= bytesParsed;
			data += bytesParsed;
			}
		}

	if (http.iserror()) {
		//{{{  error
		cLog::log (LOGERROR, "Error parsing data");
		closesocket (socket);
		return -1;
		}
		//}}}

	closesocket (socket);

	cLog::log (LOGINFO, "Response: %d", response.code);
	if (!response.body.empty())
		cLog::log (LOGINFO, "%s", &response.body[0]);

	return 0;
	}
