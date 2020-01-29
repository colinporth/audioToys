#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <iostream>
#include <string>
#include <fstream>

#include "tinywav.h"

using namespace std;
using std::string;
using std::fstream;

typedef struct WAV_HEADER {
  char                RIFF[4];        // RIFF Header      Magic header
  unsigned long       ChunkSize;      // RIFF Chunk Size
  char                WAVE[4];        // WAVE Header
  char                fmt[4];         // FMT header
  unsigned long       Subchunk1Size;  // Size of the fmt chunk
  unsigned short      AudioFormat;    // Audio format 1=PCM,6=mulaw,7=alaw, 257=IBM Mu-Law, 258=IBM A-Law, 259=ADPCM
  unsigned short      NumOfChan;      // Number of channels 1=Mono 2=Sterio
  unsigned long       SamplesPerSec;  // Sampling Frequency in Hz
  unsigned long       bytesPerSec;    // bytes per second
  unsigned short      blockAlign;     // 2=16-bit mono, 4=16-bit stereo
  unsigned short      bitsPerSample;  // Number of bits per sample
  char                Subchunk2ID[4]; // "data"  string
  unsigned long       Subchunk2Size;  // Sampled data length
  }wav_hdr;

// Function prototypes
//{{{
int getFileSize(FILE *inFile){
    int fileSize = 0;
    fseek(inFile,0,SEEK_END);

    fileSize=ftell(inFile);

    fseek(inFile,0,SEEK_SET);
    return fileSize;
}
//}}}

int main(int argc,char *argv[]){

  wav_hdr wavHeader;
  FILE *wavFile;
  int headerSize = sizeof(wav_hdr),filelength = 0;

  string answer;

  const char* filePath = argv[1];

  TinyWav tinyWav;
  TinyWavChannelFormat tinyWavChannelFormat = TW_INTERLEAVED;
  TinyWavSampleFormat tinyWavSampleFormat = TW_FLOAT32;
  auto res = tinywav_open_read (&tinyWav, filePath, tinyWavChannelFormat, tinyWavSampleFormat);

  auto data = malloc (1024*2*4);
  res = tinywav_read (&tinyWav, data, 1024);

  tinywav_close_read (&tinyWav);

  wavFile = fopen( filePath , "r" );
  if(wavFile == NULL){
    //{{{
    printf("Can not able to open wave file\n");
    //exit(EXIT_FAILURE);
    }
    //}}}

  fread(&wavHeader,headerSize,1,wavFile);
  filelength = getFileSize(wavFile);
  fclose(wavFile);

  cout << "File is                    :" << filelength << " bytes." << endl;

  cout << "RIFF header                :" << wavHeader.RIFF[0]
                                          << wavHeader.RIFF[1]
                                          << wavHeader.RIFF[2]
                                          << wavHeader.RIFF[3] << endl;

  cout << "WAVE header                :" << wavHeader.WAVE[0]
                                          << wavHeader.WAVE[1]
                                          << wavHeader.WAVE[2]
                                          << wavHeader.WAVE[3]
                                          << endl;

  cout << "FMT                        :" << wavHeader.fmt[0]
                                          << wavHeader.fmt[1]
                                          << wavHeader.fmt[2]
                                          << wavHeader.fmt[3]
                                          << endl;

  cout << "Data size                  :" << wavHeader.ChunkSize << endl;

  // Display the sampling Rate form the header
  cout << "Sampling Rate              :" << wavHeader.SamplesPerSec << endl;
  cout << "Number of bits used        :" << wavHeader.bitsPerSample << endl;
  cout << "Number of channels         :" << wavHeader.NumOfChan << endl;
  cout << "Number of bytes per second :" << wavHeader.bytesPerSec << endl;
  cout << "Data length                :" << wavHeader.Subchunk2Size << endl;
  cout << "Audio Format               :" << wavHeader.AudioFormat << endl;
  // Audio format 1=PCM,6=mulaw,7=alaw, 257=IBM Mu-Law, 258=IBM A-Law, 259=ADPCM


  cout << "Block align                :" << wavHeader.blockAlign << endl;

  cout << "Data string                :" << wavHeader.Subchunk2ID[0]
                                          << wavHeader.Subchunk2ID[1]
                                          << wavHeader.Subchunk2ID[2]
                                          << wavHeader.Subchunk2ID[3]
                                          << endl;

  cout << endl << endl << "Try something else? (y/n)";
  cin >> answer;
  //cin.get();
  cout << endl << endl;


  getchar();
  return 0;
  }
