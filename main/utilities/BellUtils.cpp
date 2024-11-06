#include "BellUtils.h"
#include "MDNSService.h"

#include <stdio.h>
#include <stdlib.h>

#include <random>    // for mt19937, uniform_int_distribution, random_device
#ifdef ESP_PLATFORM
#include "esp_system.h"
#if __has_include("esp_mac.h")
#include "esp_mac.h"
#endif
#elif defined(_WIN32) || defined(_WIN64)
#include <iphlpapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "IPHLPAPI.lib")
#pragma comment(lib, "Ws2_32.lib")
#endif

std::string bell::generateRandomUUID() {
  static std::random_device dev;
  static std::mt19937 rng(dev());

  std::uniform_int_distribution<int> dist(0, 15);

  const char* v = "0123456789abcdef";
  const bool dash[] = {0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0};

  std::string res;
  for (int i = 0; i < 16; i++) {
    if (dash[i])
      res += "-";
    res += v[dist(rng)];
    res += v[dist(rng)];
  }
  return res;
}

std::string bell::getMacAddress() {
#if defined(ESP_PLATFORM)

  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char macStr[18];
  sprintf(macStr, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2],
          mac[3], mac[4], mac[5]);
  return std::string(macStr);
#elif defined(_WIN32) || defined(_WIN64)
  ULONG outBufLen = 15000;
  IP_ADAPTER_ADDRESSES* adapterAddresses = (IP_ADAPTER_ADDRESSES*)malloc(outBufLen);

  // Get the adapter addresses
  ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
  DWORD dwRetVal = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, adapterAddresses, &outBufLen);

  if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
    free(adapterAddresses);
    adapterAddresses = (IP_ADAPTER_ADDRESSES*)malloc(outBufLen);
    dwRetVal = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, adapterAddresses, &outBufLen);
  }

  std::string activeIP = "No active IP found";
  if (dwRetVal == NO_ERROR) {
    for (IP_ADAPTER_ADDRESSES* adapter = adapterAddresses; adapter != NULL; adapter = adapter->Next) {
      // Check if the adapter is operational (up and running)
      if (adapter->OperStatus == IfOperStatusUp) {
        for (IP_ADAPTER_UNICAST_ADDRESS* unicast = adapter->FirstUnicastAddress; unicast != NULL; unicast = unicast->Next) {
          char addressBuffer[INET6_ADDRSTRLEN] = {0};
          getnameinfo(unicast->Address.lpSockaddr, unicast->Address.iSockaddrLength, 
                      addressBuffer, sizeof(addressBuffer), NULL, 0, NI_NUMERICHOST);
          
          // Assuming you prefer an IPv4 address over IPv6 (you can add specific checks here)
          if (unicast->Address.lpSockaddr->sa_family == AF_INET) {
            activeIP = std::string(addressBuffer);
            break;  // Stop once we find the first IPv4 in-use IP
          }
        }
      }
      if (activeIP != "No active IP found") {
        break;  // Break if we found an active IP
      }
    }
  }

  free(adapterAddresses);
  return activeIP;
#endif
  return "00:00:00:00:00:00";
}

void bell::freeAndNull(void*& ptr) {
  free(ptr);
  ptr = nullptr;
}
