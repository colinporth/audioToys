// minimal winSock http get parser, based on tinyHttp
#pragma once
//{{{  includes
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <winsock2.h>
#include <WS2tcpip.h>

#include <string>
#include <vector>

#include "tinyHttp.h"

#include "../../shared/utils/cLog.h"
//}}}

class cWinHttp : public cTinyHttp {
public:
  //{{{
  cWinHttp() : cTinyHttp() {
    WSAStartup (MAKEWORD(2,2), &wsaData);
    }
  //}}}
  ~cWinHttp() {}

  //{{{
  bool get (const std::string& host, const std::string& path) {

    mSocket = connectSocket (host, 80);
    if (mSocket < 0) {
      cLog::log (LOGERROR, "failed to connect socket" + host);
      return false;
      }

    std::string request = "GET /" + path + " HTTP/1.1\r\nHost: " + host + "\r\n\r\n";
    if (send (mSocket, request.c_str(), (int)request.size(), 0) != (int)request.size()) {
       //{{{  error
       cLog::log (LOGERROR, "request send failed");
       closesocket (mSocket);
       return false;
       }
       //}}}

     bool ok = true;
     while (ok) {
       char buffer[1024];
       int bytesReceived = recv (mSocket, buffer, sizeof(buffer), 0);
       if (bytesReceived <= 0) {
         //{{{  error
         cLog::log (LOGERROR, "Error receiving data");
         closesocket (mSocket);
         return false;
         }
         //}}}

       const char* data = buffer;
       while (ok && bytesReceived) {
         int bytesParsed;
         ok = parseData (data, bytesReceived, &bytesParsed);
         cLog::log (LOGINFO, "%d %d %d", ok, bytesReceived, bytesParsed);
         bytesReceived -= bytesParsed;
         data += bytesParsed;
         }
       }

     if (isError()) {
       //{{{  error
       cLog::log (LOGERROR, "Error parsing data");
       closesocket (mSocket);
       return false;
       }
       //}}}

     closesocket (mSocket);
     return true;
     }
  //}}}
  //{{{
  std::string getRedirectable (const std::string& host, const std::string& path) {

    if (get (host, path))
      if (getResponseCode() == 302)
        if (get (mRedirectUrl->host, path))
          if (getResponseCode() == 200)
            return mRedirectUrl->host;
          else
            cLog::log (LOGERROR, "cWinHttp - redirect error");

    return host;
    }
  //}}}

  int getResponseCode() { return mResponseCode; }
  int getBodySize() { return (int)mBody.size(); }
  char* getBody() { return &mBody[0]; }

protected:
  void gotHeader (const char* ckey, int nkey, const char* cvalue, int nvalue) {}
  void gotCode (int code) { mResponseCode = code; }
  void gotBody (const char* data, int size) { mBody.insert (mBody.end(), data, data + size); }

  int mSocket = -1;
  int mResponseCode = 0;
  std::vector<char> mBody;

private:
  //{{{
  int connectSocket (const std::string& host, int port) {
  // return a socket connected to a hostname, or -1

    if ((mSocket == -1) || (host != mLastHost)) {
      // not connected or different host
      //{{{  close any open webSocket
      if (mSocket >= 0)
        closesocket (mSocket);
      //}}}

      // win32 find host ipAddress
      struct addrinfo hints;
      memset (&hints, 0, sizeof(hints));
      hints.ai_family = AF_INET;       // IPv4
      hints.ai_protocol = IPPROTO_TCP; // TCP
      hints.ai_socktype = SOCK_STREAM; // TCP so its SOCK_STREAM
      struct addrinfo* targetAddressInfo = NULL;
      unsigned long getAddrRes = getaddrinfo (host.c_str(), NULL, &hints, &targetAddressInfo);
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
      mSocket = (unsigned int)socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
      if (mSocket < 0)
        return -2;

      // win32 connect webSocket
      if (connect (mSocket, (SOCKADDR*)&sockAddr, sizeof(sockAddr))) {
        closesocket (mSocket);
        mSocket = -1;
        return -3;
        }

      mLastHost = host;
      }

    return mSocket;
    }
  //}}}

  WSADATA wsaData;
  std::string mLastHost;
  };
