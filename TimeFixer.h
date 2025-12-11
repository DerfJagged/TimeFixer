//Thanks to www.free60.org/SMC
//Thanks to tmbinc for smc.c

#pragma once
#ifndef _TimeFixer_H
#define _TimeFixer_H

#include <string.h>

//Call to SMC message function in xboxkrnl.lib
extern "C" void __stdcall HalSendSMCMessage(void* input, void* output);
extern "C" void __stdcall HalReturnToFirmware(int type);

void GetRTCDate();
void SetRTCDate();
int hex_to_bytes(const char *hex, unsigned char *out, size_t max_len);
time_t decode_smc_rtc(const unsigned char *data);
void encode_smc_rtc(time_t t, unsigned char out[7]);
void print_bytes(const unsigned char *input_bytes, int len);
void RunTest();
void ShowTextForSeconds(const wchar_t *message, float seconds);

//This is used to store our SMC data
unsigned char m_SMCMessage[16];
unsigned char m_SMCReturn[16];
unsigned char m_SMCEditable[16];
void PrepareBuffers();

#endif
