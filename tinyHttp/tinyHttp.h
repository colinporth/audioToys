#pragma once
//{{{  includes
#include <stdio.h>
#include <stdlib.h>

#include <ctype.h>
#include <string.h>
#include <algorithm>

#include <sys/types.h>
//}}}

class cTinyHttp {
public:
  //{{{
  cTinyHttp() {

    mScratch = 0;
    mCode = 0;
    mParseState = 0;
    mContentLength = -1;
    mState = eStateHeader;
    mScratchSize = 0;
    mNumKey = 0;
    mNumValue = 0;
    mChunked = false;
    }
  //}}}
  //{{{
  virtual ~cTinyHttp() {

    free (mScratch);
    mScratch = 0;
    }
  //}}}

  int isError() { return mState == eStateError; }

  //{{{
  int parseData (const char* data, int size, int* read) {

    const int initial_size = size;
    while (size) {
      switch (mState) {
        case eStateHeader:
          switch (parseHeaderChar (&mParseState, *data)) {
            //{{{
            case eHeaderStatus_done:
              gotCode (mCode);
              if (mParseState != 0)
                mState = eStateError;
              else if (mChunked) {
                mContentLength = 0;
                mState = eStateChunkHeader;
                }
              else if (mContentLength == 0)
                mState = eStateClose;
              else if (mContentLength > 0)
                mState = eStateRawData;
              else if (mContentLength == -1)
                mState = eStateUnknownData;
              else
                mState = eStateError;
              break;
            //}}}
            //{{{
            case eHeaderStatus_code_character:
              mCode = mCode * 10 + *data - '0';
              break;
            //}}}
            //{{{
            case eHeaderStatus_key_character:
              growScratch (mNumKey + 1);
              mScratch[mNumKey] = tolower(*data);
              ++mNumKey;
              break;
            //}}}
            //{{{
            case eHeaderStatus_value_character:
              growScratch (mNumKey + mNumValue + 1);
              mScratch[mNumKey + mNumValue] = *data;
              ++mNumValue;
              break;
            //}}}
            //{{{
            case eHeaderStatus_store_keyvalue:
              if (mNumKey == 17 && strncmp (mScratch, "transfer-encoding", mNumKey) == 0)
                mChunked = (mNumValue == 7 && strncmp (mScratch + mNumKey, "chunked", mNumValue) == 0);

              else if (mNumKey == 14 && strncmp (mScratch, "content-length", mNumKey) == 0) {
                int ii, end;
                mContentLength = 0;
                for (ii = mNumKey, end = mNumKey + mNumValue; ii != end; ++ii)
                  mContentLength = mContentLength * 10 + mScratch[ii] - '0';
                }

              else if ((mNumKey == 8) && (strncmp (mScratch, "location", mNumKey) == 0)) {
                if (!mRedirectUrl)
                  mRedirectUrl = new cUrl();
                mRedirectUrl->parse (mScratch + mNumKey, mNumKey + mNumValue);
                }

              gotHeader (mScratch, mNumKey, mScratch + mNumKey, mNumValue);

              mNumKey = 0;
              mNumValue = 0;
              break;
            //}}}
            }
          --size;
          ++data;
          break;

        //{{{
        case eStateChunkHeader:
          if (!parseChunked (&mParseState, &mContentLength, *data)) {
            if (mContentLength == -1)
              mState = eStateError;
            else if (mContentLength == 0)
              mState = eStateClose;
            else
              mState = eStateChunkData;
            }

          --size;
          ++data;
          break;
        //}}}
        //{{{
        case eStateChunkData: {
          const int chunksize = std::min (size, mContentLength);
          gotBody (data, chunksize);
          mContentLength -= chunksize;
          size -= chunksize;
          data += chunksize;

          if (mContentLength == 0) {
            mContentLength = 1;
            mState = eStateChunkHeader;
            }
          }
        break;
        //}}}
        //{{{
        case eStateRawData: {
          const int chunksize = std::min (size, mContentLength);
          gotBody (data, chunksize);

          mContentLength -= chunksize;
           size -= chunksize;
          data += chunksize;

          if (mContentLength == 0)
            mState = eStateClose;
          }

        break;
        //}}}
        //{{{
        case eStateUnknownData: {
          if (size == 0)
            mState = eStateClose;
          else {
            gotBody (data, size);
            size -= size;
            data += size;
            }
          }
        break;
        //}}}

        case eStateClose:
        case eStateError:
          break;
        }

      if (mState == eStateError || mState == eStateClose) {
        free (mScratch);
        mScratch = 0;
        *read = initial_size - size;
        return 0;
        }
      }

    *read = initial_size - size;
    return 1;
    }
  //}}}

protected:
  virtual void gotBody (const char* data, int size) = 0;
  virtual void gotHeader (const char* ckey, int nkey, const char* cvalue, int nvalue) = 0;
  virtual void gotCode (int code) = 0;
  //{{{
  void clear() {

    mState = eStateHeader;
    mCode = 0;
    mParseState = 0;
    mContentLength = -1;
    mNumKey = 0;
    mNumValue = 0;
    mChunked = false;

    free (mScratch);
    mScratch = nullptr;
    mScratchSize = 0;
    }
  //}}}

  //{{{
  class cUrl {
  public:
    //{{{
    cUrl() : scheme(nullptr), host(nullptr), path(nullptr), port(nullptr),
                   username(nullptr), password(nullptr), query(nullptr), fragment(nullptr) {}
    //}}}
    //{{{
    ~cUrl() {

      free (scheme);
      free (host);
      free (port);
      free (query);
      free (fragment);
      free (username);
      free (password);
      }
    //}}}
    //{{{
    void parse (const char* url, int urlLen) {
    // parseUrl, see RFC 1738, 3986

      auto curstr = url;
      //{{{  parse scheme
      // <scheme>:<scheme-specific-part>
      // <scheme> := [a-z\+\-\.]+
      //             upper case = lower case for resiliency
      const char* tmpstr = strchr (curstr, ':');
      if (!tmpstr)
        return;
      auto len = tmpstr - curstr;

      // Check restrictions
      for (auto i = 0; i < len; i++)
        if (!isalpha (curstr[i]) && ('+' != curstr[i]) && ('-' != curstr[i]) && ('.' != curstr[i]))
          return;

      // Copy the scheme to the storage
      scheme = (char*)malloc (len+1);
      strncpy_s (scheme, len + 1, curstr, len);
      scheme[len] = '\0';

      // Make the character to lower if it is upper case.
      for (auto i = 0; i < len; i++)
        scheme[i] = tolower (scheme[i]);
      //}}}

      // skip ':'
      tmpstr++;
      curstr = tmpstr;
      //{{{  parse user, password
      // <user>:<password>@<host>:<port>/<url-path>
      // Any ":", "@" and "/" must be encoded.
      // Eat "//" */
      for (auto i = 0; i < 2; i++ ) {
        if ('/' != *curstr )
          return;
        curstr++;
        }

      // Check if the user (and password) are specified
      auto userpass_flag = 0;
      tmpstr = curstr;
      while (tmpstr < url + urlLen) {
        if ('@' == *tmpstr) {
          // Username and password are specified
          userpass_flag = 1;
         break;
          }
        else if ('/' == *tmpstr) {
          // End of <host>:<port> specification
          userpass_flag = 0;
          break;
          }
        tmpstr++;
        }

      // User and password specification
      tmpstr = curstr;
      if (userpass_flag) {
        //{{{  Read username
        while ((tmpstr < url + urlLen) && (':' != *tmpstr) && ('@' != *tmpstr))
           tmpstr++;

        len = tmpstr - curstr;
        username = (char*)malloc(len+1);
        strncpy_s(username, len+1, curstr, len);
        username[len] = '\0';
        //}}}
        // Proceed current pointer
        curstr = tmpstr;
        if (':' == *curstr) {
          // Skip ':'
          curstr++;
          //{{{  Read password
          tmpstr = curstr;
          while ((tmpstr < url + urlLen) && ('@' != *tmpstr))
            tmpstr++;

          len = tmpstr - curstr;
          password = (char*)malloc(len+1);
          strncpy_s (password, len+1, curstr, len);
          password[len] = '\0';
          curstr = tmpstr;
          }
          //}}}

        // Skip '@'
        if ('@' != *curstr)
          return;
        curstr++;
        }
      //}}}

      auto bracket_flag = ('[' == *curstr) ? 1 : 0;
      //{{{  parse host
      tmpstr = curstr;
      while (tmpstr < url + urlLen) {
        if (bracket_flag && ']' == *tmpstr) {
          // End of IPv6 address
          tmpstr++;
          break;
          }
        else if (!bracket_flag && (':' == *tmpstr || '/' == *tmpstr))
          // Port number is specified
          break;
        tmpstr++;
        }

      len = tmpstr - curstr;
      host = (char*)malloc(len+1);
      strncpy_s(host, len+1, curstr, len);
      host[len] = '\0';
      curstr = tmpstr;
      //}}}
      //{{{  parse port number
      if (':' == *curstr) {
        curstr++;

        // Read port number
        tmpstr = curstr;
        while ((tmpstr < url + urlLen) && ('/' != *tmpstr))
          tmpstr++;

        len = tmpstr - curstr;
        port = (char*)malloc(len+1);
        strncpy_s(port, len+1, curstr, len);
        port[len] = '\0';
        curstr = tmpstr;
        }
      //}}}

      // end of string ?
      if (curstr >= url + urlLen)
        return;

      //{{{  Skip '/'
      if ('/' != *curstr)
        return;

      curstr++;
      //}}}
      //{{{  Parse path
      tmpstr = curstr;
      while ((tmpstr < url + urlLen) && ('#' != *tmpstr) && ('?' != *tmpstr))
        tmpstr++;

      len = tmpstr - curstr;
      path = (char*)malloc(len+1);
      strncpy_s(path, len+1,  curstr, len);
      path[len] = '\0';
      curstr = tmpstr;
      //}}}
      //{{{  parse query
      if ('?' == *curstr) {
        // skip '?'
        curstr++;

        /* Read query */
        tmpstr = curstr;
        while ((tmpstr < url + urlLen) && ('#' != *tmpstr))
          tmpstr++;
        len = tmpstr - curstr;

        query = (char*)malloc(len+1);
        strncpy_s(query, len+1, curstr, len);
        query[len] = '\0';
        curstr = tmpstr;
        }
      //}}}
      //{{{  parse fragment
      if ('#' == *curstr) {
        // Skip '#'
        curstr++;

        /* Read fragment */
        tmpstr = curstr;
        while (tmpstr < url + urlLen)
          tmpstr++;
        len = tmpstr - curstr;

        fragment = (char*)malloc(len+1);
        strncpy_s(fragment, len+1, curstr, len);
        fragment[len] = '\0';

        curstr = tmpstr;
        }
      //}}}
      }
    //}}}
    //{{{  public vars
    char* scheme;    // mandatory
    char* host;      // mandatory
    char* path;      // optional
    char* port;      // optional
    char* username;  // optional
    char* password;  // optional
    char* query;     // optional
    char* fragment;  // optional
    //}}}
    };
  //}}}
  cUrl* mRedirectUrl = nullptr;

private:
  //{{{
  void growScratch (int size) {

    if (mScratchSize >= size)
      return;

    if (size < 64)
      size = 64;
    int nsize = (mScratchSize * 3) / 2;
    if (nsize < size)
      nsize = size;

    mScratch = (char*)realloc (mScratch, nsize);
    mScratchSize = nsize;
    }
  //}}}

  //{{{
  enum eState {
    eStateHeader,
    eStateChunkHeader,
    eStateChunkData,
    eStateRawData,
    eStateUnknownData,
    eStateClose,
    eStateError,
    };
  //}}}
  //{{{
  enum eHeaderStatus {
    eHeaderStatus_done,
    eHeaderStatus_continue,
    eHeaderStatus_version_character,
    eHeaderStatus_code_character,
    eHeaderStatus_status_character,
    eHeaderStatus_key_character,
    eHeaderStatus_value_character,
    eHeaderStatus_store_keyvalue
    };
  //}}}

  //{{{
  inline const static unsigned char chunk_state[] = {
  // *    LF    CR    HEX
    0xC1, 0xC1, 0xC1,    1,  // s0: initial hex char
    0xC1, 0xC1,    2, 0x81,  // s1: additional hex chars, followed by CR
    0xC1, 0x83, 0xC1, 0xC1,  // s2: trailing LF */
    0xC1, 0xC1,    4, 0xC1,  // s3: CR after chunk block
    0xC1, 0xC0, 0xC1, 0xC1,  // s4: LF after chunk block
    };
  //}}}
  //{{{
  int parseChunked (int* state, int* size, char ch) {

    int newstate, code = 0;
    switch (ch) {
      case '\n': code = 1; break;
      case '\r': code = 2; break;
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
      case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
      case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': code = 3; break;
      }

    newstate = chunk_state[*state * 4 + code];
    *state = (newstate & 0xF);

    switch (newstate) {
      case 0xC0:
        return *size != 0;

      case 0xC1: /* error */
        *size = -1;
        return 0;

      case 0x01: /* initial char */
        *size = 0;
        /* fallthrough */
      case 0x81: /* size char */
        if (ch >= 'a')
          *size = *size * 16 + (ch - 'a' + 10);
        else if (ch >= 'A')
          *size = *size * 16 + (ch - 'A' + 10);
        else
          *size = *size * 16 + (ch - '0');
        break;

      case 0x83:
          return *size == 0;
      }

    return 1;
    }
  //}}}

  //{{{
  inline const static unsigned char header_state[] = {
  //  *    \t    \n   \r    ' '     ,     :   PAD
    0x80,    1, 0xC1, 0xC1,    1, 0x80, 0x80, 0xC1, // state 0:  HTTP version
    0x81,    2, 0xC1, 0xC1,    2,    1,    1, 0xC1, // state 1:  Response code
    0x82, 0x82,    4,    3, 0x82, 0x82, 0x82, 0xC1, // state 2:  Response reason
    0xC1, 0xC1,    4, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, // state 3:  HTTP version newline
    0x84, 0xC1, 0xC0,    5, 0xC1, 0xC1,    6, 0xC1, // state 4:  Start of header field
    0xC1, 0xC1, 0xC0, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, // state 5:  Last CR before end of header
    0x87,    6, 0xC1, 0xC1,    6, 0x87, 0x87, 0xC1, // state 6:  leading whitespace before header value
    0x87, 0x87, 0xC4,   10, 0x87, 0x88, 0x87, 0xC1, // state 7:  header field value
    0x87, 0x88,    6,    9, 0x88, 0x88, 0x87, 0xC1, // state 8:  Split value field value
    0xC1, 0xC1,    6, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, // state 9:  CR after split value field
    0xC1, 0xC1, 0xC4, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, // state 10: CR after header value
    };
  //}}}
  //{{{
  eHeaderStatus parseHeaderChar (int* state, char ch) {

    int newstate, code = 0;
    switch (ch) {
      case '\t': code = 1; break;
      case '\n': code = 2; break;
      case '\r': code = 3; break;
      case  ' ': code = 4; break;
      case  ',': code = 5; break;
      case  ':': code = 6; break;
      }

    newstate = header_state[*state * 8 + code];
    *state = (newstate & 0xF);

    switch (newstate) {
      case 0xC0: return eHeaderStatus_done;
      case 0xC1: return eHeaderStatus_done;
      case 0xC4: return eHeaderStatus_store_keyvalue;
      case 0x80: return eHeaderStatus_version_character;
      case 0x81: return eHeaderStatus_code_character;
      case 0x82: return eHeaderStatus_status_character;
      case 0x84: return eHeaderStatus_key_character;
      case 0x87: return eHeaderStatus_value_character;
      case 0x88: return eHeaderStatus_value_character;
      }

    return eHeaderStatus_continue;
    }
  //}}}

  eState mState;

  int mCode;
  int mParseState;
  int mContentLength;
  int mNumKey;
  int mNumValue;
  bool mChunked;

  int mScratchSize;
  char* mScratch;
  };
