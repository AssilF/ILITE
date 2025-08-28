#include "espnow_discovery.h"

// Broadcast MAC address used for discovery messages.
static const uint8_t kBroadcastMac[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

// Simple discovery payload. More fields can be added in the future.
struct DiscoveryMessage {
    uint8_t type = 0x01;
};

void EspNowDiscovery::begin() {
    // Ensure station + AP mode so OTA soft AP remains active.
    // Record any existing peers (e.g. pre-configured target).
    esp_now_peer_num_t count{};
    if (esp_now_get_peer_num(&count) == ESP_OK) {
        peerCount = count.total_num;
    }
}

void EspNowDiscovery::discover() {
    DiscoveryMessage msg;
    esp_now_send(kBroadcastMac, reinterpret_cast<const uint8_t*>(&msg), sizeof(msg));
}

void EspNowDiscovery::handleIncoming(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (len < sizeof(DiscoveryMessage)) {
        return; // Ignore unknown packets
    }

    // Pair with the device if not already paired.
    if (!esp_now_is_peer_exist(mac)) {
        esp_now_peer_info_t peerInfo{};
        memcpy(peerInfo.peer_addr, mac, 6);
        peerInfo.channel = 0; // current channel
        peerInfo.encrypt = false;
        if (esp_now_add_peer(&peerInfo) == ESP_OK) {
            Serial.print("Paired with: ");
            Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            // Remember this peer for UI display.
            bool known = false;
            for (int i = 0; i < peerCount; ++i) {
                if (memcmp(peerMacs[i], mac, 6) == 0) {
                    known = true;
                    break;
                }
            }
            if (!known && peerCount < kMaxPeers) {
                memcpy(peerMacs[peerCount], mac, 6);
                peerCount++;
            }
        }
    }
}

bool EspNowDiscovery::hasPeers() const {
    return peerCount > 0;
}
