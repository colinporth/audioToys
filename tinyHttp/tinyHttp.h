#pragma once
//{{{  includes
#include <ctype.h>
#include <string.h>
#include <algorithm>
//}}}

class cTinyHttp {
public:
  //{{{
  struct http_funcs {
    // realloc_scratch - reallocate memory, cannot fail. There will only
    //                   be one scratch buffer. Implemnentation may take advantage of this fact.
    void* (*realloc_scratch)(void* opaque, void* ptr, int size);

    // body - handle HTTP response body data
    void (*body)(void* opaque, const char* data, int size);

    // header - handle an HTTP header key/value pair
    void (*header)(void* opaque, const char* key, int nkey, const char* value, int nvalue);

    // code - handle the HTTP status code for the response
    void (*code)(void* opqaue, int code);
    };
  //}}}

  //{{{
  cTinyHttp (struct http_funcs funcs, void* opaque) {

    this->funcs = funcs;
    scratch = 0;
    this->opaque = opaque;
    code = 0;
    parsestate = 0;
    contentlength = -1;
    state = http_roundtripper_header;
    nscratch = 0;
    nkey = 0;
    nvalue = 0;
    chunked = 0;
    }
  //}}}
  //{{{
  ~cTinyHttp() {

    if (scratch) {
      funcs.realloc_scratch(opaque, scratch, 0);
      scratch = 0;
      }
    }
  //}}}

int iserror() { return state == http_roundtripper_error; }

//{{{
int parseData (const char* data, int size, int* read) {

  const int initial_size = size;
  while (size) {
    switch (state) {
      case http_roundtripper_header:
        switch (http_parse_header_char(&parsestate, *data)) {
          //{{{
          case http_header_status_done:
              funcs.code(opaque, code);
              if (parsestate != 0)
                  state = http_roundtripper_error;
              else if (chunked) {
                  contentlength = 0;
                  state = http_roundtripper_chunk_header;
              } else if (contentlength == 0)
                  state = http_roundtripper_close;
              else if (contentlength > 0)
                  state = http_roundtripper_raw_data;
              else if (contentlength == -1)
                  state = http_roundtripper_unknown_data;
              else
                  state = http_roundtripper_error;
              break;
          //}}}
          //{{{
          case http_header_status_code_character:
            code = code * 10 + *data - '0';
            break;
          //}}}
          //{{{
          case http_header_status_key_character:
            growScratch (nkey + 1);
            scratch[nkey] = tolower(*data);
            ++nkey;
            break;
          //}}}
          //{{{
          case http_header_status_value_character:
            growScratch (nkey + nvalue + 1);
            scratch[nkey+nvalue] = *data;
            ++nvalue;
            break;
          //}}}
          //{{{
          case http_header_status_store_keyvalue:
            if (nkey == 17 && 0 == strncmp(scratch, "transfer-encoding", nkey))
              chunked = (nvalue == 7 && 0 == strncmp(scratch + nkey, "chunked", nvalue));
            else if (nkey == 14 && 0 == strncmp(scratch, "content-length", nkey)) {
              int ii, end;
              contentlength = 0;
              for (ii = nkey, end = nkey + nvalue; ii != end; ++ii)
                contentlength = contentlength * 10 + scratch[ii] - '0';
              }
            funcs.header(opaque, scratch, nkey, scratch + nkey, nvalue);
            nkey = 0;
            nvalue = 0;
            break;
          //}}}
          }
        --size;
        ++data;
        break;

      //{{{
      case http_roundtripper_chunk_header:
        if (!http_parse_chunked(&parsestate, &contentlength, *data)) {
          if (contentlength == -1)
            state = http_roundtripper_error;
          else if (contentlength == 0)
            state = http_roundtripper_close;
          else
            state = http_roundtripper_chunk_data;
          }
        --size;
        ++data;
        break;
      //}}}
      //{{{
      case http_roundtripper_chunk_data: {
        const int chunksize = std::min (size, contentlength);
        appendBody (data, chunksize);
        contentlength -= chunksize;
        size -= chunksize;
        data += chunksize;

        if (contentlength == 0) {
          contentlength = 1;
          state = http_roundtripper_chunk_header;
          }
        }
      break;
      //}}}
      //{{{
      case http_roundtripper_raw_data: {
        const int chunksize = std::min (size, contentlength);
        appendBody (data, chunksize);
        contentlength -= chunksize;
         size -= chunksize;
        data += chunksize;
        if (contentlength == 0)
          state = http_roundtripper_close;
        }
      break;
      //}}}
      //{{{
      case http_roundtripper_unknown_data: {
        if (size == 0)
          state = http_roundtripper_close;
        else {
          appendBody (data, size);
          size -= size;
          data += size;
          }
        }
      break;
      //}}}

      case http_roundtripper_close:
      case http_roundtripper_error:
        break;
      }

    if (state == http_roundtripper_error || state == http_roundtripper_close) {
      if (scratch) {
         funcs.realloc_scratch(opaque, scratch, 0);
         scratch = 0;
        }
      *read = initial_size - size;
      return 0;
      }
    }

  *read = initial_size - size;
  return 1;
  }
//}}}

private:
  //{{{
  void appendBody (const char* data, int ndata) {
    funcs.body (opaque, data, ndata);
    }
  //}}}
  //{{{
  void growScratch (int size) {

    if (nscratch >= size)
      return;

    if (size < 64)
      size = 64;
    int nsize = (nscratch * 3) / 2;
    if (nsize < size)
      nsize = size;

    scratch = (char*)funcs.realloc_scratch (opaque, scratch, nsize);
    nscratch = nsize;
    }
  //}}}

  //{{{
  enum http_header_status {
    http_header_status_done,
    http_header_status_continue,
    http_header_status_version_character,
    http_header_status_code_character,
    http_header_status_status_character,
    http_header_status_key_character,
    http_header_status_value_character,
    http_header_status_store_keyvalue
    };
  //}}}
  //{{{
  enum http_roundtripper_state {
    http_roundtripper_header,
    http_roundtripper_chunk_header,
    http_roundtripper_chunk_data,
    http_roundtripper_raw_data,
    http_roundtripper_unknown_data,
    http_roundtripper_close,
    http_roundtripper_error,
    };
  //}}}

  //{{{
  inline const static unsigned char http_chunk_state[] = {
  // *    LF    CR    HEX
    0xC1, 0xC1, 0xC1,    1, // s0: initial hex char */
    0xC1, 0xC1,    2, 0x81, // s1: additional hex chars, followed by CR */
    0xC1, 0x83, 0xC1, 0xC1, // s2: trailing LF */
    0xC1, 0xC1,    4, 0xC1, // s3: CR after chunk block */
    0xC1, 0xC0, 0xC1, 0xC1, // s4: LF after chunk block */
    };
  //}}}
  //{{{
  int http_parse_chunked (int* state, int* size, char ch) {

    int newstate, code = 0;
    switch (ch) {
      case '\n': code = 1; break;
      case '\r': code = 2; break;
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
      case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
      case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': code = 3; break;
      }

    newstate = http_chunk_state[*state * 4 + code];
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
  inline const static unsigned char http_header_state[] = {
  //  *    \t    \n   \r    ' '     ,     :   PAD
    0x80,    1, 0xC1, 0xC1,    1, 0x80, 0x80, 0xC1, // state 0: HTTP version
    0x81,    2, 0xC1, 0xC1,    2,    1,    1, 0xC1, // state 1: Response code
    0x82, 0x82,    4,    3, 0x82, 0x82, 0x82, 0xC1, // state 2: Response reason
    0xC1, 0xC1,    4, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, // state 3: HTTP version newline
    0x84, 0xC1, 0xC0,    5, 0xC1, 0xC1,    6, 0xC1, // state 4: Start of header field
    0xC1, 0xC1, 0xC0, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, //state 5: Last CR before end of header
    0x87,    6, 0xC1, 0xC1,    6, 0x87, 0x87, 0xC1, // state 6: leading whitespace before header value
    0x87, 0x87, 0xC4,   10, 0x87, 0x88, 0x87, 0xC1, // state 7: header field value
    0x87, 0x88,    6,    9, 0x88, 0x88, 0x87, 0xC1, // state 8: Split value field value
    0xC1, 0xC1,    6, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, // state 9: CR after split value field
    0xC1, 0xC1, 0xC4, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, // state 10:CR after header value
    };
  //}}}
  //{{{
  int http_parse_header_char (int* state, char ch) {

    int newstate, code = 0;
    switch (ch) {
      case '\t': code = 1; break;
      case '\n': code = 2; break;
      case '\r': code = 3; break;
      case  ' ': code = 4; break;
      case  ',': code = 5; break;
      case  ':': code = 6; break;
      }

    newstate = http_header_state[*state * 8 + code];
    *state = (newstate & 0xF);

    switch (newstate) {
      case 0xC0: return http_header_status_done;
      case 0xC1: return http_header_status_done;
      case 0xC4: return http_header_status_store_keyvalue;
      case 0x80: return http_header_status_version_character;
      case 0x81: return http_header_status_code_character;
      case 0x82: return http_header_status_status_character;
      case 0x84: return http_header_status_key_character;
      case 0x87: return http_header_status_value_character;
      case 0x88: return http_header_status_value_character;
      }

    return http_header_status_continue;
    }
  //}}}

  struct http_funcs funcs;
  void* opaque;
  char* scratch;
  int code;
  int parsestate;
  int contentlength;
  int state;
  int nscratch;
  int nkey;
  int nvalue;
  int chunked;
  };
