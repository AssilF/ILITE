#include "espnow_discovery.h"
#include "audio_feedback.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr uint8_t kBroadcastMac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

void logMac(const char* prefix, const uint8_t* mac) {
    char buffer[24] = {};
    EspNowDiscovery::macToString(mac, buffer, sizeof(buffer));
    Serial.print(prefix);
    Serial.println(buffer);
}

}  // namespace

void EspNowDiscovery::begin() {
    WiFi.mode(WIFI_AP_STA);
    esp_err_t chanResult = esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
    if (chanResult != ESP_OK) {
        Serial.print("[ESP-NOW] Failed to set WiFi channel: ");
        Serial.println(chanResult);
    }

    fillSelfIdentity();

    // Register the broadcast peer if it is not already present.
    if (!esp_now_is_peer_exist(kBroadcastMac)) {
        esp_now_peer_info_t peerInfo{};
        memcpy(peerInfo.peer_addr, kBroadcastMac, sizeof(peerInfo.peer_addr));
        peerInfo.channel = WIFI_CHANNEL;
        peerInfo.encrypt = false;
        if (esp_now_add_peer(&peerInfo) != ESP_OK) {
            Serial.println("[ESP-NOW] Failed to add broadcast peer");
        }
    }

    for (int i = 0; i < kMaxPeers; ++i) {
        peers[i] = PeerEntry{};
    }
    peerCount = 0;
    link = LinkState{};
    lastBroadcastMs = 0;

    Serial.print("[ESP-NOW] Role: ");
#if DEVICE_ROLE == DEVICE_ROLE_CONTROLLER
    Serial.println("controller");
#else
    Serial.println("controlled");
#endif

    char idString[64] = {};
    snprintf(idString, sizeof(idString), "%s/%s (%s)", selfIdentity.customId,
             selfIdentity.platform, selfIdentity.deviceType);
    Serial.print("[ESP-NOW] Identity: ");
    Serial.println(idString);
}

void EspNowDiscovery::discover() {
    uint32_t now = millis();
    pruneExpiredPeers(now);

#if DEVICE_ROLE == DEVICE_ROLE_CONTROLLER
    if (now - lastBroadcastMs >= BROADCAST_INTERVAL_MS) {
        sendPacket(MessageType::MSG_PAIR_REQ, kBroadcastMac);
        lastBroadcastMs = now;
    }

    if (!link.paired) {
        int targetIndex = selectTarget();
        if (targetIndex >= 0) {
            PeerEntry& target = peers[targetIndex];
            if (!target.confirmed || now - link.lastConfirmSentMs >= BROADCAST_INTERVAL_MS) {
                if (sendPacket(MessageType::MSG_PAIR_CONFIRM, target.mac)) {
                    target.confirmed = true;
                    link.peerIndex = targetIndex;
                    link.awaitingAck = true;
                    link.lastConfirmSentMs = now;
                    memcpy(link.peerMac, target.mac, sizeof(link.peerMac));
                    Serial.print("[ESP-NOW] Sent confirm to ");
                    logMac("", target.mac);
                }
            }
        }
    } else {
        if (now - link.lastActivityMs > LINK_TIMEOUT_MS) {
            Serial.println("[ESP-NOW] Link timeout, resetting");
            resetLink();
        } else if (now - link.lastKeepAliveSentMs >= BROADCAST_INTERVAL_MS) {
            if (sendPacket(MessageType::MSG_KEEPALIVE, link.peerMac)) {
                link.lastKeepAliveSentMs = now;
            }
        }
    }
#else
    if (link.paired) {
        if (now - link.lastActivityMs > LINK_TIMEOUT_MS) {
            Serial.println("[ESP-NOW] Controller timeout, resetting link");
            resetLink();
        } else if (now - link.lastKeepAliveSentMs >= BROADCAST_INTERVAL_MS) {
            if (sendPacket(MessageType::MSG_KEEPALIVE, link.peerMac)) {
                link.lastKeepAliveSentMs = now;
            }
        }
    }
#endif
}

bool EspNowDiscovery::handleIncoming(const uint8_t* mac, const uint8_t* incomingData, int len) {
    if (len < static_cast<int>(sizeof(Packet))) {
        return false;
    }

    const Packet* packet = reinterpret_cast<const Packet*>(incomingData);
    if (packet->version != kProtocolVersion) {
        return false;
    }

    uint32_t now = millis();
    MessageType type = static_cast<MessageType>(packet->type);

    switch (type) {
        case MessageType::MSG_PAIR_REQ:
#if DEVICE_ROLE == DEVICE_ROLE_CONTROLLED
            Serial.println("[ESP-NOW] Pair request received");
            upsertPeer(packet->id, mac, now);
            ensurePeer(mac);
            sendPacket(MessageType::MSG_IDENTITY_REPLY, mac);
            audioFeedback(AudioCue::PeerRequest);
            return true;
#else
            return false;
#endif

        case MessageType::MSG_IDENTITY_REPLY:
#if DEVICE_ROLE == DEVICE_ROLE_CONTROLLER
            Serial.println("[ESP-NOW] Identity reply received");
            upsertPeer(packet->id, mac, now);
            ensurePeer(mac);
            audioFeedback(AudioCue::PeerDiscovered);
            return true;
#else
            return false;
#endif

        case MessageType::MSG_PAIR_CONFIRM:
#if DEVICE_ROLE == DEVICE_ROLE_CONTROLLED
            Serial.println("[ESP-NOW] Pair confirm received");
            ensurePeer(mac);
            int index = upsertPeer(packet->id, mac, now);
            if (index >= 0) {
                link.paired = true;
                link.peerIndex = index;
                memcpy(link.peerMac, mac, sizeof(link.peerMac));
                link.lastActivityMs = now;
                link.lastKeepAliveSentMs = now;
                sendPacket(MessageType::MSG_PAIR_ACK, mac);
                peers[index].acked = true;
                Serial.println("[ESP-NOW] Paired with controller");
                audioFeedback(AudioCue::PeerAcknowledge);
            }
            return true;
#else
            return false;
#endif

        case MessageType::MSG_PAIR_ACK:
#if DEVICE_ROLE == DEVICE_ROLE_CONTROLLER
            if (link.awaitingAck && macEqual(mac, link.peerMac)) {
                Serial.println("[ESP-NOW] Pair ack received");
                link.paired = true;
                link.awaitingAck = false;
                link.lastActivityMs = now;
                link.lastKeepAliveSentMs = now;
                int index = findPeerIndex(mac);
                if (index >= 0) {
                    peers[index].acked = true;
                    peers[index].lastSeen = now;
                    peerCount = computePeerCount();
                }
                audioFeedback(AudioCue::PeerAcknowledge);
                return true;
            }
            return false;
#else
            return false;
#endif

        case MessageType::MSG_KEEPALIVE:
            if (link.paired && macEqual(mac, link.peerMac)) {
                link.lastActivityMs = now;
                int index = findPeerIndex(mac);
                if (index >= 0) {
                    peers[index].lastSeen = now;
                }
                return true;
            }
            return false;
    }

    return false;
}

bool EspNowDiscovery::hasPeers() const {
    return peerCount > 0;
}

const uint8_t* EspNowDiscovery::getPeer(int index) const {
    if (index < 0 || index >= kMaxPeers) {
        return nullptr;
    }
    return peers[index].inUse ? peers[index].mac : nullptr;
}

const char* EspNowDiscovery::getPeerName(int index) const {
    if (index < 0 || index >= kMaxPeers) {
        return "";
    }
    return peers[index].inUse ? peers[index].identity.customId : "";
}

int EspNowDiscovery::findPeerIndex(const uint8_t* mac) const {
    if (!mac) {
        return -1;
    }
    for (int i = 0; i < kMaxPeers; ++i) {
        if (peers[i].inUse && macEqual(peers[i].mac, mac)) {
            return i;
        }
    }
    return -1;
}

void EspNowDiscovery::macToString(const uint8_t* mac, char* buffer, size_t bufferLen) {
    if (!buffer || bufferLen < 18) {
        return;
    }
    if (!mac) {
        snprintf(buffer, bufferLen, "00:00:00:00:00:00");
        return;
    }
    snprintf(buffer, bufferLen, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1],
             mac[2], mac[3], mac[4], mac[5]);
}

bool EspNowDiscovery::macEqual(const uint8_t* a, const uint8_t* b) {
    return a && b && memcmp(a, b, 6) == 0;
}

void EspNowDiscovery::fillSelfIdentity() {
    memset(&selfIdentity, 0, sizeof(selfIdentity));
#if DEVICE_ROLE == DEVICE_ROLE_CONTROLLER
    strncpy(selfIdentity.deviceType, "controller", sizeof(selfIdentity.deviceType) - 1);
#else
    strncpy(selfIdentity.deviceType, "controlled", sizeof(selfIdentity.deviceType) - 1);
#endif
    strncpy(selfIdentity.platform, DEVICE_PLATFORM, sizeof(selfIdentity.platform) - 1);
    strncpy(selfIdentity.customId, DEVICE_CUSTOM_ID, sizeof(selfIdentity.customId) - 1);
    WiFi.macAddress(selfIdentity.mac);
}

bool EspNowDiscovery::ensurePeer(const uint8_t* mac) const {
    if (!mac) {
        return false;
    }
    if (esp_now_is_peer_exist(mac)) {
        return true;
    }
    esp_now_peer_info_t peerInfo{};
    memcpy(peerInfo.peer_addr, mac, 6);
    peerInfo.channel = WIFI_CHANNEL;
    peerInfo.encrypt = false;
    esp_err_t err = esp_now_add_peer(&peerInfo);
    if (err != ESP_OK) {
        Serial.print("[ESP-NOW] Failed to add peer: ");
        Serial.println(err);
        return false;
    }
    return true;
}

bool EspNowDiscovery::sendPacket(MessageType type, const uint8_t* mac) {
    if (!mac) {
        return false;
    }

    Packet packet{};
    packet.version = kProtocolVersion;
    packet.type = static_cast<uint8_t>(type);
    packet.id = selfIdentity;
    WiFi.macAddress(packet.id.mac);
    memcpy(selfIdentity.mac, packet.id.mac, sizeof(selfIdentity.mac));
    packet.monotonicMs = millis();
    memset(packet.reserved, 0, sizeof(packet.reserved));

    if (!macEqual(mac, kBroadcastMac)) {
        if (!ensurePeer(mac)) {
            return false;
        }
    }

    esp_err_t err = esp_now_send(mac, reinterpret_cast<const uint8_t*>(&packet), sizeof(packet));
    if (err != ESP_OK) {
        Serial.print("[ESP-NOW] Send failed: ");
        Serial.println(err);
        return false;
    }
    return true;
}

int EspNowDiscovery::upsertPeer(const Identity& id, const uint8_t* mac, uint32_t now) {
    if (!mac) {
        return -1;
    }

    for (int i = 0; i < kMaxPeers; ++i) {
        if (peers[i].inUse && macEqual(peers[i].mac, mac)) {
            peers[i].identity = id;
            memcpy(peers[i].mac, mac, 6);
            peers[i].lastSeen = now;
            peerCount = computePeerCount();
            return i;
        }
    }

    for (int i = 0; i < kMaxPeers; ++i) {
        if (!peers[i].inUse) {
            peers[i].inUse = true;
            peers[i].identity = id;
            memcpy(peers[i].mac, mac, 6);
            peers[i].lastSeen = now;
            peers[i].confirmed = false;
            peers[i].acked = false;
            peerCount = computePeerCount();
            char label[24] = {};
            macToString(mac, label, sizeof(label));
            Serial.print("[ESP-NOW] Discovered peer: ");
            Serial.print(id.customId);
            Serial.print(" @ ");
            Serial.println(label);
            return i;
        }
    }

    Serial.println("[ESP-NOW] Peer table full");
    return -1;
}

void EspNowDiscovery::pruneExpiredPeers(uint32_t now) {
    bool changed = false;
    for (int i = 0; i < kMaxPeers; ++i) {
        if (peers[i].inUse && now - peers[i].lastSeen > DEVICE_TTL_MS) {
            char label[24] = {};
            macToString(peers[i].mac, label, sizeof(label));
            Serial.print("[ESP-NOW] Removing stale peer: ");
            Serial.println(label);
            if (link.peerIndex == i) {
                resetLink();
            }
            peers[i] = PeerEntry{};
            changed = true;
        }
    }
    if (changed) {
        peerCount = computePeerCount();
    }
}

int EspNowDiscovery::selectTarget() const {
    int fallback = -1;
    for (int i = 0; i < kMaxPeers; ++i) {
        if (!peers[i].inUse) {
            continue;
        }
        if (!peers[i].acked) {
            return i;
        }
        if (fallback < 0) {
            fallback = i;
        }
    }
    return fallback;
}

void EspNowDiscovery::resetLink() {
    if (link.peerIndex >= 0 && link.peerIndex < kMaxPeers) {
        peers[link.peerIndex].acked = false;
        peers[link.peerIndex].confirmed = false;
    }
    link = LinkState{};
}

int EspNowDiscovery::computePeerCount() const {
    int count = 0;
    for (int i = 0; i < kMaxPeers; ++i) {
        if (peers[i].inUse) {
            ++count;
        }
    }
    return count;
}
