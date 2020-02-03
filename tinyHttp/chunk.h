#pragma once
//{{{
#if defined(__cplusplus)
  extern "C" {
#endif
//}}}

int http_parse_chunked (int* state, int *size, char ch);

//{{{
#if defined(__cplusplus)
  }
#endif
//}}}
