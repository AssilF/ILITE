#ifndef ESPNOW_DISCOVERY_H
#define ESPNOW_DISCOVERY_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

// Simple ESP-NOW discovery helper that broadcasts a discovery
// message and pairs with any responding peers.
class EspNowDiscovery {
public:
    // Ensure WiFi remains in AP+STA mode so OTA soft AP stays available;
    // ESP-NOW should already be initialised by the caller.
    void begin();
    // Broadcast a discovery message to any nearby devices.
    void discover();
    // Examine an incoming packet and, if it is part of the discovery
    // handshake, process it. Returns true if the packet was consumed by the
    // discovery system and should not be treated as application data.
    bool handleIncoming(const uint8_t *mac, const uint8_t *incomingData, int len);
    // Return true if at least one peer has been paired.
    bool hasPeers() const;
    // Retrieve the number of paired peers.
      int  getPeerCount() const { return peerCount; }
      // Get the MAC address of a paired peer by index.
      const uint8_t* getPeer(int index) const { return peerMacs[index]; }
      // Get the identity name of a paired peer by index.
      const char* getPeerName(int index) const { return peerNames[index]; }

private:
      static constexpr int kMaxPeers = 5;
      uint8_t peerMacs[kMaxPeers][6] = {};
      bool    peerAcked[kMaxPeers] = {};
      int     peerCount = 0;
      char    peerNames[kMaxPeers][16] = {};
  };

#endif // ESPNOW_DISCOVERY_H
