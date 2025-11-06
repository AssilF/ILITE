#include "espnow_discovery.h"
#include "audio_feedback.h"
#include "connection_log.h"
#if DEVICE_ROLE == DEVICE_ROLE_CONTROLLER
#include "display.h"
#endif

#include <cstdio>
#include <cstring>

// Timeout for link inactivity in milliseconds
namespace {

constexpr uint8_t kBroadcastMac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

const char* messageTypeToString(MessageType type) {
    switch (type) {
        case MessageType::MSG_PAIR_REQ:
            return "MSG_PAIR_REQ";
        case MessageType::MSG_IDENTITY_REPLY:
            return "MSG_IDENTITY_REPLY";
        case MessageType::MSG_PAIR_CONFIRM:
            return "MSG_PAIR_CONFIRM";
        case MessageType::MSG_PAIR_ACK:
            return "MSG_PAIR_ACK";
        case MessageType::MSG_COMMAND:
            return "MSG_COMMAND";
        default:
            return "MSG_UNKNOWN";
    }
}

const char* macLabel(const uint8_t* mac, char* buffer, size_t bufferLen) {
    if (!buffer || bufferLen == 0) {
        return "";
    }
    if (!mac) {
        snprintf(buffer, bufferLen, "(null)");
        return buffer;
    }
    if (EspNowDiscovery::macEqual(mac, kBroadcastMac)) {
        snprintf(buffer, bufferLen, "broadcast");
        return buffer;
    }
    EspNowDiscovery::macToString(mac, buffer, bufferLen);
    return buffer;
}

void logTx(MessageType type, const uint8_t* mac) {
    char label[24] = {};
    connectionLogAddf("TX %s to %s", messageTypeToString(type), macLabel(mac, label, sizeof(label)));
}

void logRx(MessageType type, const uint8_t* mac) {
    char label[24] = {};
    connectionLogAddf("RX %s from %s", messageTypeToString(type), macLabel(mac, label, sizeof(label)));
}

void logMac(const char* prefix, const uint8_t* mac) {
    char buffer[24] = {};
    EspNowDiscovery::macToString(mac, buffer, sizeof(buffer));
    Serial.print(prefix);
    Serial.println(buffer);
}

}  // namespace

// Global pointer for static callback access
static EspNowDiscovery* g_discoveryInstance = nullptr;

// Static ESP-NOW receive callback (forwards to instance method)
static void onEspNowDataRecv(const uint8_t* mac, const uint8_t* incomingData, int len) {
    if (g_discoveryInstance != nullptr) {
        g_discoveryInstance->handleIncoming(mac, incomingData, len);
    }
}

void EspNowDiscovery::begin() {
    WiFi.mode(WIFI_AP_STA);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    // esp_err_t chanResult = esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE); //we're not setting channels for now, let them play loose.
    // if (chanResult != ESP_OK) {
    //     Serial.print("[ESP-NOW] Failed to set WiFi channel: ");
    //     Serial.println(chanResult);
    //     connectionLogAddf("WiFi channel error: %d", chanResult);
    // }

    fillSelfIdentity();
    connectionLogAdd("ESP-NOW begin");
#if DEVICE_ROLE == DEVICE_ROLE_CONTROLLER
    connectionLogAdd("Role: controller");
#else
    connectionLogAdd("Role: controlled");
#endif
    connectionLogAddf("Identity: %s/%s", selfIdentity.customId, selfIdentity.deviceType);

    // Register ESP-NOW receive callback
    g_discoveryInstance = this;
    esp_now_register_recv_cb(onEspNowDataRecv);
    Serial.println("[ESP-NOW] Receive callback registered");

    // Register the broadcast peer if it is not already present.
    if (!esp_now_is_peer_exist(kBroadcastMac)) {
        esp_now_peer_info_t peerInfo{};
        memcpy(peerInfo.peer_addr, kBroadcastMac, sizeof(peerInfo.peer_addr));
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
    discoveryEnabled = true;

    Serial.print("[ESP-NOW] Role: ");
#if DEVICE_ROLE == DEVICE_ROLE_CONTROLLER
    Serial.println("controller");
#else
    Serial.println("controlled");
#endif

    char idString[32] = {};
    snprintf(idString, sizeof(idString), "%s/%s (%s)", selfIdentity.customId,
             selfIdentity.platform, selfIdentity.deviceType);
    Serial.print("[ESP-NOW] Identity: ");
    Serial.println(idString);
}

void EspNowDiscovery::discover() {
    uint32_t now = millis();
    pruneExpiredPeers(now);

#if DEVICE_ROLE == DEVICE_ROLE_CONTROLLER
    // Note: Old display mode system removed - now using discoveryEnabled flag
    bool allowBroadcast = discoveryEnabled;
    bool shouldBroadcast = allowBroadcast && !link.paired;

    if (shouldBroadcast && now - lastBroadcastMs >= BROADCAST_INTERVAL_MS) {
        char mac[24];
        macLabel(kBroadcastMac, mac, sizeof(mac));
        Serial.printf("[ESP-NOW] Broadcasting PAIR_REQ to %s\n", mac);
        sendPacket(MessageType::MSG_PAIR_REQ, kBroadcastMac);
        lastBroadcastMs = now;
    }

    // Auto-pair with first discovered device
    bool autoPairEnabled = discoveryEnabled;
    if (autoPairEnabled && !link.paired) {
        int targetIndex = selectTarget();
        if (targetIndex >= 0) {
            PeerEntry& target = peers[targetIndex];
            bool shouldConfirm = !target.confirmed || now - link.lastConfirmSentMs >= BROADCAST_INTERVAL_MS;
            if (shouldConfirm) {
                Serial.printf("[ESP-NOW] Auto-pairing with device index %d\n", targetIndex);
                beginPairingWith(target.mac);
            }
        }
    } else if (link.paired) {
        if (now - link.lastActivityMs > LINK_TIMEOUT_MS) {
            Serial.println("[ESP-NOW] Link timeout, resetting");
            resetLink();
            connectionLogAdd("Link timeout, resetting");
        }
    }
#else
    if (link.paired) {
        if (now - link.lastActivityMs > LINK_TIMEOUT_MS) {
            Serial.println("[ESP-NOW] Controller timeout, resetting link");
            resetLink();
            connectionLogAdd("Controller timeout, resetting link");
        }
    }
#endif
}

bool EspNowDiscovery::handleIncoming(const uint8_t* mac, const uint8_t* incomingData, int len) {

    const Packet* packet = reinterpret_cast<const Packet*>(incomingData);
    uint32_t now = millis();
    MessageType type = packet->type;
    logRx(type, mac);

    // Serial logging for RX (similar to TX logging)
    Serial.printf("[RX] Received %s (%d bytes) from %02X:%02X:%02X:%02X:%02X:%02X\n",
        messageTypeToString(type), len,
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    if (len < static_cast<int>(sizeof(Packet))) {
        Serial.printf("[ESP-NOW] Packet too small: %d < %d bytes\n", len, sizeof(Packet));
        return false;
    }

    if (packet->version != kProtocolVersion) {
        Serial.printf("[ESP-NOW] Protocol version mismatch: %d != %d\n", packet->version, kProtocolVersion);
        return false;
    }

    if (link.paired && macEqual(mac, link.peerMac)) {
        link.lastActivityMs = now;
        int index = findPeerIndex(mac);
        if (index >= 0) {
            peers[index].lastSeen = now;
        }
    }

    switch (type) {
        case MessageType::MSG_PAIR_REQ:
#if DEVICE_ROLE == DEVICE_ROLE_CONTROLLED
            if (link.paired) {
                connectionLogAdd("Ignoring pair request while paired");
                return true;
            }
            Serial.println("[ESP-NOW] Pair request received");
            char pairLabel[24] = {};
            macToString(mac, pairLabel, sizeof(pairLabel));
            connectionLogAddf("Pair request from %s", pairLabel);
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
            connectionLogAddf("Recieved an identity");
            upsertPeer(packet->id, mac, now);
            ensurePeer(mac);
            audioFeedback(AudioCue::PeerDiscovered);
            connectionLogAddf("Identity reply: %s", packet->id.customId);
            return true;
#else
            return false;
#endif

        case MessageType::MSG_PAIR_CONFIRM:
#if DEVICE_ROLE == DEVICE_ROLE_CONTROLLED
            Serial.println("[ESP-NOW] Pair confirm received");
            char confirmLabel[24] = {};
            macToString(mac, confirmLabel, sizeof(confirmLabel));
            connectionLogAddf("Pair confirm from %s", confirmLabel);
            ensurePeer(mac);
            int index = upsertPeer(packet->id, mac, now);
            if (index >= 0) {
                link.paired = true;
                link.peerIndex = index;
                memcpy(link.peerMac, mac, sizeof(link.peerMac));
                link.lastActivityMs = now;
                sendPacket(MessageType::MSG_PAIR_ACK, mac);
                peers[index].acked = true;
                Serial.println("[ESP-NOW] Paired with controller");
                audioFeedback(AudioCue::PeerAcknowledge);
                connectionLogAddf("Paired with %s", packet->id.customId);
            }
            return true;
#else
            return false;
#endif

        case MessageType::MSG_PAIR_ACK:
#if DEVICE_ROLE == DEVICE_ROLE_CONTROLLER
            if (link.awaitingAck && macEqual(mac, link.peerMac)) {
                Serial.println("[ESP-NOW] Pair ack received");
                connectionLogAddf("Acked PAIR");
                char ackLabel[24] = {};
                macToString(mac, ackLabel, sizeof(ackLabel));
                link.paired = true;
                link.awaitingAck = false;
                link.lastActivityMs = now;
                int index = findPeerIndex(mac);
                if (index >= 0) {
                    peers[index].acked = true;
                    peers[index].lastSeen = now;
                    peerCount = computePeerCount();
                }
                discoveryEnabled = false;
                audioFeedback(AudioCue::PeerAcknowledge);
                connectionLogAddf("Pair ack from %s", ackLabel);
                return true;
            }
            return false;
#else
            return false;
#endif

        case MessageType::MSG_COMMAND:
            if (len >= static_cast<int>(sizeof(CommandPacket))) {
                const CommandPacket* cmd = reinterpret_cast<const CommandPacket*>(incomingData);
                char commandLabel[24] = {};
                macToString(mac, commandLabel, sizeof(commandLabel));
                connectionLogAddf("Command from %s: %s", commandLabel, cmd->command);
                if (commandCallback) {
                    char message[sizeof(cmd->command) + 1];
                    memcpy(message, cmd->command, sizeof(cmd->command));
                    message[sizeof(cmd->command)] = '\0';
                    commandCallback(message);
                }
            } else {
                connectionLogAdd("Command packet truncated");
            }
            return true;
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
    peerInfo.encrypt = false;
    esp_err_t err = esp_now_add_peer(&peerInfo);
    if (err != ESP_OK) {
        Serial.print("[ESP-NOW] Failed to add peer: ");
        connectionLogAddf("[ESPN]Failed to add peer");
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
    packet.type = type;
    packet.id = selfIdentity;
    WiFi.macAddress(packet.id.mac);
    memcpy(selfIdentity.mac, packet.id.mac, sizeof(selfIdentity.mac));
    packet.monotonicMs = millis();
    memset(&packet.reserved, 0, sizeof(packet.reserved));

    if (!macEqual(mac, kBroadcastMac)) {
        if (!ensurePeer(mac)) {
            return false;
        }
    }

    esp_err_t err = esp_now_send(mac, reinterpret_cast<const uint8_t*>(&packet), sizeof(packet));
    if (err != ESP_OK) {
        Serial.print("[ESP-NOW] Send failed: ");
        Serial.println(err);
        connectionLogAddf("Send failed (%u): %d", static_cast<unsigned>(type), err);
        return false;
    }
    logTx(type, mac);
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
            connectionLogAddf("Peer updated: %s", id.customId);
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
            connectionLogAddf("Peer discovered: %s", id.customId);
            return i;
        }
    }

    Serial.println("[ESP-NOW] Peer table full");
    connectionLogAdd("Peer table full");
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
            connectionLogAddf("Peer stale: %s", label);
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
    discoveryEnabled = true;
    lastBroadcastMs = 0;
    connectionLogAdd("Link reset");
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

bool EspNowDiscovery::sendCommand(const uint8_t* mac, const char* command) {
    if (!mac || !command) {
        return false;
    }

    CommandPacket packet{};
    packet.header.version = kProtocolVersion;
    packet.header.type = MessageType::MSG_COMMAND;
    packet.header.id = selfIdentity;
    WiFi.macAddress(packet.header.id.mac);
    memcpy(selfIdentity.mac, packet.header.id.mac, sizeof(selfIdentity.mac));
    packet.header.monotonicMs = millis();
    memset(&packet.header.reserved, 0, sizeof(packet.header.reserved));
    strncpy(packet.command, command, sizeof(packet.command) - 1);
    packet.command[sizeof(packet.command) - 1] = '\0';

    if (!macEqual(mac, kBroadcastMac)) {
        if (!ensurePeer(mac)) {
            connectionLogAdd("Command target unavailable");
            return false;
        }
    }

    esp_err_t err = esp_now_send(mac, reinterpret_cast<const uint8_t*>(&packet), sizeof(packet));
    if (err != ESP_OK) {
        Serial.print("[ESP-NOW] Command send failed: ");
        Serial.println(err);
        connectionLogAddf("Command send failed: %d", err);
        return false;
    }

    char label[24] = {};
    macToString(mac, label, sizeof(label));
    logTx(MessageType::MSG_COMMAND, mac);
    connectionLogAddf("Command sent to %s: %s", label, packet.command);
    return true;
}

void EspNowDiscovery::resetLinkState() {
    resetLink();
}

bool EspNowDiscovery::isPaired() const {
    return link.paired;
}

int EspNowDiscovery::getPairedPeerIndex() const {
    return link.paired ? link.peerIndex : -1;
}

const uint8_t* EspNowDiscovery::getPairedMac() const {
    return link.paired ? link.peerMac : nullptr;
}

const Identity* EspNowDiscovery::getPairedIdentity() const {
    if (!link.paired || link.peerIndex < 0 || link.peerIndex >= kMaxPeers) {
        return nullptr;
    }
    const PeerEntry& entry = peers[link.peerIndex];
    if (!entry.inUse) {
        return nullptr;
    }
    return &entry.identity;
}

void EspNowDiscovery::setDiscoveryEnabled(bool enabled) {
    discoveryEnabled = enabled;
    if (enabled && !link.paired) {
        lastBroadcastMs = 0;
    }
}

bool EspNowDiscovery::isDiscoveryEnabled() const {
    return discoveryEnabled;
}

void EspNowDiscovery::setCommandCallback(void (*callback)(const char* message)) {
    commandCallback = callback;
}

bool EspNowDiscovery::beginPairingWith(const uint8_t* mac) {
    if (!mac) {
        return false;
    }
    int index = findPeerIndex(mac);
    if (index < 0) {
        return false;
    }
    if (link.paired && macEqual(mac, link.peerMac)) {
        return true;
    }
    if (link.paired && !macEqual(mac, link.peerMac)) {
        resetLink();
    }
    if (!ensurePeer(mac)) {
        return false;
    }
    uint32_t now = millis();
    if (sendPacket(MessageType::MSG_PAIR_CONFIRM, mac)) {
        peers[index].confirmed = true;
        peers[index].acked = false;
        link.peerIndex = index;
        link.awaitingAck = true;
        link.lastConfirmSentMs = now;
        memcpy(link.peerMac, mac, sizeof(link.peerMac));
        Serial.print("[ESP-NOW] Sent confirm to ");
        logMac("", mac);
        connectionLogAddf("Pair confirm -> %s", peers[index].identity.customId);
        return true;
    }
    return false;
}
