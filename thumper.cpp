// thumper.cpp 

// Sweep power off / on glitches -- controlled by ScrollLock LED

// TODO: test on latest code, best shipped and connected


#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <stdio.h>
#include <math.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Winmm.lib")

const int PowerControlKey = VK_NUMLOCK;  // VK_NUMLOCK, VK_SCROLL, (VK_CAPITAL)
const char* IPAddrStr = "192.168.1.147"; // address to ping  -- set static DHCP lease on AP Services

const int MinPingWaitMs = 500; // minimum ping wait time (Windowze)
const int MaxBootWaitMs = 90 * 1000;

const int Hz = 60;
const float ZeroCrossingSec = 1.f / Hz / 2;  // for higher precision timing use non-zero-crossing optoisolator


void sleep(float sec) {
  LARGE_INTEGER ft = {.QuadPart = (int)(-10000000 * sec)}; // negative = relative 100 ns ticks (10 MHz) 
  HANDLE timer = CreateWaitableTimer(NULL, TRUE, NULL);
  if (!timer) exit(-3);
  SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
  WaitForSingleObject(timer, INFINITE);
  CloseHandle(timer);  
}


bool getKeyState(int key) {
  BYTE keyState[256];
  if (!GetKeyboardState((BYTE*)&keyState)) exit(-4);
  return keyState[key] & 1;
}

void toggleKey(int key) {
  INPUT inputs[2] = { {INPUT_KEYBOARD, {key, KEYEVENTF_EXTENDEDKEY}},  // press key
                      {INPUT_KEYBOARD, {key, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP}} }; // release key
  while (SendInput(2, (INPUT*)inputs, sizeof(INPUT)) != 2);  // retry UIPI blocked keystrokes
}


void bootWait() {
  static HANDLE hIcmpFile = IcmpCreateFile();
  static IPAddr ipaddr = inet_addr(IPAddrStr);

  for (int ms = 0; ms < MaxBootWaitMs; ms += MinPingWaitMs) {
    const char send[] = "up";
    ICMP_ECHO_REPLY replies[8];     
    if (IcmpSendEcho2(hIcmpFile, NULL, NULL, NULL, ipaddr, (void*)send, sizeof(send), NULL, replies, sizeof(replies), MinPingWaitMs) && !strcmp((char*)replies[0].Data, send)) { // ping
      printf("%s in %4.1fs\n", (char*)replies[0].Data, ms / 1000.);
      return;
    }
  }
  printf("Hung!\a\n");
  exit(-1);
}

void reset(float offSec = 0.2f) {
  toggleKey(PowerControlKey); // off
  sleep(offSec); // reset
  toggleKey(PowerControlKey); // on
}

void sweepPowerOff(float step = ZeroCrossingSec, float maxSec = 1) {
  for (float offSec = 0; offSec <= maxSec; offSec += step) {  
    printf("Off %.3fs ", offSec);
    reset(offSec);
    bootWait();
  }
}

void sweepPowerOn(float step = ZeroCrossingSec, float minSec = 0, float maxSec = 30) {
  for (float onSec = minSec; onSec <= maxSec; onSec += step) {  
    printf("On %.3fs ", onSec);
    reset();
    sleep(onSec);  // run
    reset();
    bootWait();
  }
}


int __cdecl main(int argc, char** argv) {
  if (!getKeyState(PowerControlKey))
    toggleKey(PowerControlKey);  // set initially on

  timeBeginPeriod(1);

#if 1
  sweepPowerOn(ZeroCrossingSec, 4.8f); // resume where left off
  return 0;
#endif


  // quick
  sweepPowerOff(ZeroCrossingSec, 0.2f);
  sweepPowerOn(1);

  // intermediate
  sweepPowerOff(sqrtf(ZeroCrossingSec));
  sweepPowerOn(sqrtf(ZeroCrossingSec));

  // slow, thorough
  sweepPowerOff();  
  sweepPowerOn(); 

  timeEndPeriod(1);

  // IcmpCloseHandle(hIcmpFile);
  return 0;
}
