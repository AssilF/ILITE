#include "espnow_discovery.h"

// Broadcast MAC address used for discovery messages.
static const uint8_t kBroadcastMac[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

// Message type identifiers for the discovery handshake.
enum MessageType : uint8_t {
    kScan       = 0x01, // ILITE broadcast
    kResponse   = 0x02, // Device responds with its type + MAC
    kAck        = 0x03, // ILITE acknowledges and shares its MAC
    kAckConfirm = 0x04  // Device confirms receipt of ILITE MAC
};

// Broadcast scan message with only a type field.
struct ScanMessage {
    uint8_t type = kScan;
};

// Device response: includes its device type and MAC address.
struct ResponseMessage {
    uint8_t type      = kResponse;
    uint8_t deviceType;
    uint8_t mac[6];
};

// Acknowledgement from ILITE containing its MAC address.
struct AckMessage {
    uint8_t type = kAck;
    uint8_t mac[6];
};

// Confirmation message from the device after receiving ILITE's MAC.
struct AckConfirmMessage {
    uint8_t type = kAckConfirm;
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
    ScanMessage msg;
    esp_now_send(kBroadcastMac, reinterpret_cast<const uint8_t*>(&msg), sizeof(msg));
}

bool EspNowDiscovery::handleIncoming(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (len < 1) {
        return false; // Not a discovery message
    }

    uint8_t msgType = incomingData[0];
    if (msgType == kResponse) {
        if (len < static_cast<int>(sizeof(ResponseMessage))) {
            return true; // Malformed but consumed
        }
        const ResponseMessage* resp = reinterpret_cast<const ResponseMessage*>(incomingData);
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
                    peerAcked[peerCount] = false;
                    peerCount++;
                }
            }
        }

        // Send acknowledgement with our MAC address.
        AckMessage ack;
        WiFi.macAddress(ack.mac);
        esp_now_send(mac, reinterpret_cast<uint8_t*>(&ack), sizeof(ack));
        return true;
    } else if (msgType == kAckConfirm) {
        // Device confirms receipt of our MAC.
        for (int i = 0; i < peerCount; ++i) {
            if (memcmp(peerMacs[i], mac, 6) == 0) {
                peerAcked[i] = true;
                break;
            }
        }
        return true;
    }

    return false; // Not a discovery message
}

bool EspNowDiscovery::hasPeers() const {
    return peerCount > 0;
}

