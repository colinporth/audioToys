#pragma once

#include "header.h"

static unsigned char http_header_state[] = {
/*     *    \t    \n   \r    ' '     ,     :   PAD */
    0x80,    1, 0xC1, 0xC1,    1, 0x80, 0x80, 0xC1, /* state 0: HTTP version */
    0x81,    2, 0xC1, 0xC1,    2,    1,    1, 0xC1, /* state 1: Response code */
    0x82, 0x82,    4,    3, 0x82, 0x82, 0x82, 0xC1, /* state 2: Response reason */
    0xC1, 0xC1,    4, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, /* state 3: HTTP version newline */
    0x84, 0xC1, 0xC0,    5, 0xC1, 0xC1,    6, 0xC1, /* state 4: Start of header field */
    0xC1, 0xC1, 0xC0, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, /* state 5: Last CR before end of header */
    0x87,    6, 0xC1, 0xC1,    6, 0x87, 0x87, 0xC1, /* state 6: leading whitespace before header value */
    0x87, 0x87, 0xC4,   10, 0x87, 0x88, 0x87, 0xC1, /* state 7: header field value */
    0x87, 0x88,    6,    9, 0x88, 0x88, 0x87, 0xC1, /* state 8: Split value field value */
    0xC1, 0xC1,    6, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, /* state 9: CR after split value field */
    0xC1, 0xC1, 0xC4, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, /* state 10:CR after header value */
};

int http_parse_header_char(int* state, char ch) {
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
