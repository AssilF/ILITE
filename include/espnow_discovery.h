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
    // Examine an incoming packet and, if the sender isn't already paired,
    // add it as a new peer.
    void handleIncoming(const uint8_t *mac, const uint8_t *incomingData, int len);
    // Return true if at least one peer has been paired.
    bool hasPeers() const;
    // Retrieve the number of paired peers.
    int  getPeerCount() const { return peerCount; }
    // Get the MAC address of a paired peer by index.
    const uint8_t* getPeer(int index) const { return peerMacs[index]; }

private:
    static constexpr int kMaxPeers = 5;
    uint8_t peerMacs[kMaxPeers][6] = {};
    int peerCount = 0;
};

#endif // ESPNOW_DISCOVERY_H
