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

const char* IPAddrStr = "192.168.1.97"; // address to ping
const int MinPingWaitMs = 500; // minimum ping wait time (Windowze)
const int MaxBootWaitMs = 30 * 1000;
const int PowerControlKey = VK_SCROLL;  // VK_NUMLOCK, VK_SCROLL, (VK_CAPITAL)
const int ZeroCrossingUs = 1000 * 1000 / 60 / 2;  // 60 Hz;  for higher precision timing use non-zero-crossing optoisolator


void usleep(__int64 usec) {
  LARGE_INTEGER ft; ft.QuadPart = -10 * usec; // 100 ns ticks, negative = relative
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
  INPUT inputs[2] = {0};
  inputs[0].type = inputs[1].type = INPUT_KEYBOARD;
  inputs[0].ki.wVk = inputs[1].ki.wVk = key;
  inputs[0].ki.dwFlags = KEYEVENTF_EXTENDEDKEY;
  inputs[1].ki.dwFlags = KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP;
  if (SendInput(2, (INPUT*)inputs, sizeof(INPUT)) != 2) exit(-2);
}


void bootWait() {
  static HANDLE hIcmpFile = IcmpCreateFile();
  static IPAddr ipaddr = inet_addr(IPAddrStr);

  for (int ms = 0; ms < MaxBootWaitMs; ms += MinPingWaitMs) {
    const char send[] = "up";
    ICMP_ECHO_REPLY replies[8];     
    if (IcmpSendEcho2(hIcmpFile, NULL, NULL, NULL, ipaddr, (void*)send, sizeof(send), NULL, replies, sizeof(replies), MinPingWaitMs) && !strcmp((char*)replies[0].Data, send)) { // ping
      printf("%s in %4.1f ms\n", (char*)replies[0].Data, ms / 1000.);
      return;
    }
  }
  printf("Hung!\a\n");
  exit(-1);
}

void sweepPowerOff() {
  for (int off_us = 0; off_us < 10 * 1000 * 1000; off_us += ZeroCrossingUs) {  
    printf("%6.1f ms: ", off_us / 1000.);
    toggleKey(PowerControlKey); // off
    usleep(off_us);
    toggleKey(PowerControlKey); // on

    bootWait();
  }
}

void sweepPowerUp() {
  for (int on_us = 0; on_us < 20 * 1000 * 1000; on_us += ZeroCrossingUs) {  
    printf("%6.1f ms: ", on_us / 1000.);
    toggleKey(PowerControlKey); // off
    usleep(100 * 1000); // reset
    toggleKey(PowerControlKey); // on
    usleep(on_us);  // run
    toggleKey(PowerControlKey); // off
    usleep(100 * 1000); // reset
    toggleKey(PowerControlKey); // on

    bootWait();
  }
}


int __cdecl main(int argc, char** argv) {
  if (!getKeyState(VK_SCROLL))
    toggleKey(VK_SCROLL);  // set initially on

  timeBeginPeriod(1);

  sweepPowerUp();
  sweepPowerOff();

  timeEndPeriod(1);

  // IcmpCloseHandle(hIcmpFile);
}
