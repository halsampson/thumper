// thumper.cpp 

// Sweep power off / on glitches -- controlled by ScrollLock LED

// TODO: test on latest code, best shipped and connected


#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <stdio.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Winmm.lib")

const int PowerControlKey = VK_SCROLL;  // VK_NUMLOCK, VK_SCROLL, (VK_CAPITAL)

const char* IPAddrStr = "192.168.1.97"; // address to ping
const int MinPingWaitMs = 500; // minimum ping wait time (Windowze)
const int MaxBootWaitMs = 30 * 1000;

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
  INPUT inputs[2] = { {INPUT_KEYBOARD, {key, KEYEVENTF_EXTENDEDKEY}}, {INPUT_KEYBOARD, {key, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP}} };
  if (SendInput(2, (INPUT*)inputs, sizeof(INPUT)) != 2) exit(-2);
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

void sweepPowerOff(float step = ZeroCrossingSec, float maxSec = 1) {
  for (float offSec = 0; offSec < maxSec; offSec += step) {  
    printf("Off %.3fs ", offSec);
    toggleKey(PowerControlKey); // off
    sleep(offSec);
    toggleKey(PowerControlKey); // on

    bootWait();
  }
}

void sweepPowerUp(float step = ZeroCrossingSec, float maxSec = 30, float minSec = 0) {
  for (float onSec = minSec; onSec < maxSec; onSec += step) {  
    printf("On %.3fs ", onSec);
    toggleKey(PowerControlKey); // off
    sleep(0.1f); // reset
    toggleKey(PowerControlKey); // on
    sleep(onSec);  // run
    toggleKey(PowerControlKey); // off
    sleep(0.1f); // reset
    toggleKey(PowerControlKey); // on

    bootWait();
  }
}


int __cdecl main(int argc, char** argv) {
  if (!getKeyState(VK_SCROLL))
    toggleKey(VK_SCROLL);  // set initially on

  timeBeginPeriod(1);

  sweepPowerUp(1);
  sweepPowerOff();  

  sweepPowerUp(ZeroCrossingSec, 30, 7.7f); // resume

  timeEndPeriod(1);

  // IcmpCloseHandle(hIcmpFile);
}
