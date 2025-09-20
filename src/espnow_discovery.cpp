#include "espnow_discovery.h"
#include <cstring>
#include <Arduino.h>

// Broadcast MAC address used for discovery messages.
static const uint8_t kBroadcastMac[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

enum PairingType : uint8_t {
    SCAN_REQUEST  = 0x01,
    DRONE_IDENTITY = 0x02,
    ILITE_IDENTITY = 0x03,
    DRONE_ACK      = 0x04
};

struct IdentityMessage {
    uint8_t type;
    char identity[16];
    uint8_t mac[6];
} __attribute__((packed));

static const char kControllerId[] = "ILITEA1";

void EspNowDiscovery::begin() {
    // Ensure station + AP mode so OTA soft AP remains active.
    // Record any existing peers (e.g. pre-configured target).
    esp_now_peer_num_t count{};
    if (esp_now_get_peer_num(&count) == ESP_OK) {
        peerCount = count.total_num;
    }
}

void EspNowDiscovery::discover() {
    IdentityMessage msg{};
    msg.type = SCAN_REQUEST;
    Serial.println("Sending discovery request");
    esp_now_send(kBroadcastMac, reinterpret_cast<const uint8_t*>(&msg), sizeof(msg));
}

bool EspNowDiscovery::handleIncoming(const uint8_t *mac, const uint8_t *incomingData, int len) {
    // Ignore any packets originating from the universal broadcast address
    // or our own MAC address. The controller occasionally receives its own
    // broadcast discovery frames and would otherwise attempt to treat them
    // as peer telemetry or even pair with itself.
    if (memcmp(mac, kBroadcastMac, 6) == 0) {
        return false; // Ignore broadcast traffic
    }
    uint8_t selfMac[6];
    WiFi.macAddress(selfMac);
    if (memcmp(mac, selfMac, 6) == 0) {
        return false; // Ignore packets we originated
    }

    if (len != sizeof(IdentityMessage)) {
        return false; // Not a pairing message
    }

    const IdentityMessage* msg = reinterpret_cast<const IdentityMessage*>(incomingData);

    if (msg->type == DRONE_IDENTITY) {
        if (!esp_now_is_peer_exist(mac)) {
            esp_now_peer_info_t peerInfo{};
            memcpy(peerInfo.peer_addr, mac, 6);
            peerInfo.channel = 0;
            peerInfo.encrypt = false;
            if (esp_now_add_peer(&peerInfo) == ESP_OK) {
                Serial.print("Paired with: ");
                Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                bool known = false;
                for (int i = 0; i < peerCount; ++i) {
                    if (memcmp(peerMacs[i], mac, 6) == 0) {
                        known = true;
                        strncpy(peerNames[i], msg->identity, sizeof(peerNames[i])-1);
                        peerNames[i][sizeof(peerNames[i])-1] = '\0';
                        break;
                    }
                }
                if (!known && peerCount < kMaxPeers) {
                    memcpy(peerMacs[peerCount], mac, 6);
                    peerAcked[peerCount] = false;
                    strncpy(peerNames[peerCount], msg->identity, sizeof(peerNames[peerCount])-1);
                    peerNames[peerCount][sizeof(peerNames[peerCount])-1] = '\0';
                    peerCount++;
                }
            }
        }

        IdentityMessage resp{};
        resp.type = ILITE_IDENTITY;
        strncpy(resp.identity, kControllerId, sizeof(resp.identity));
        WiFi.macAddress(resp.mac);
        esp_now_send(mac, reinterpret_cast<uint8_t*>(&resp), sizeof(resp));
        Serial.println("Sending Controller ID ");
        return true;
    } else if (msg->type == DRONE_ACK) {
        Serial.println("Drone Acked");
        for (int i = 0; i < peerCount; ++i) {
            if (memcmp(peerMacs[i], mac, 6) == 0) {
                peerAcked[i] = true;
                break;
            }
        }
        return true;
    }

    return false; // Not a pairing message
}

bool EspNowDiscovery::hasPeers() const {
    return peerCount > 0;
}

