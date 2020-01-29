#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <stdlib.h>

#include "tinywav.h"

using namespace std;

int main (int argc,char *argv[]){

 const char* filePath = argv[1];

  cTinyWav tinyWav;
  auto res = tinyWav.openRead (filePath, cTinyWav::TW_FLOAT32, cTinyWav::TW_INTERLEAVED);
  auto data = malloc (1024*2*4);
  res = tinyWav.read (data, 1024);
  tinyWav.closeRead();


  return 0;
  }
