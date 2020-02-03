#pragma once
//{{{
#if defined(__cplusplus)
  extern "C" {
#endif
//}}}

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

// * Parses a single character of an HTTP header stream. The state parameter is
// * used as internal state and should be initialized to zero for the first call.
// * Return value is a value from the http_header_status enuemeration specifying
// * the semantics of the character. If an error is encountered,
// * http_header_status_done will be returned with a non-zero state parameter. On
// * success http_header_status_done is returned with the state parameter set to
// * zero.
int http_parse_header_char(int* state, char ch);

//{{{
#if defined(__cplusplus)
  }
#endif
//}}}
