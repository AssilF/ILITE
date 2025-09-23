#ifndef ESPNOW_DISCOVERY_H
#define ESPNOW_DISCOVERY_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// -----------------------------------------------------------------------------
// Compile-time configuration
// -----------------------------------------------------------------------------

#define DEVICE_ROLE_CONTROLLER 1
#define DEVICE_ROLE_CONTROLLED 2

#ifndef DEVICE_ROLE
#define DEVICE_ROLE DEVICE_ROLE_CONTROLLER
#endif

#ifndef DEVICE_PLATFORM
#define DEVICE_PLATFORM "ILITE"
#endif

#ifndef DEVICE_CUSTOM_ID
#define DEVICE_CUSTOM_ID "ILITE-A10"
#endif

// ESP-NOW discovery and pairing parameters.
static constexpr uint8_t WIFI_CHANNEL = 6;
static constexpr uint32_t BROADCAST_INTERVAL_MS = 1000;
static constexpr uint32_t DEVICE_TTL_MS = 10'000;
static constexpr uint32_t LINK_TIMEOUT_MS = 5'000;

// -----------------------------------------------------------------------------
// Identity & packet layout
// -----------------------------------------------------------------------------

#pragma pack(push, 1)
struct Identity {
    char deviceType[12];
    char platform[16];
    char customId[32];
    uint8_t mac[6];
};

enum class MessageType : uint8_t {
    MSG_PAIR_REQ = 0x01,
    MSG_IDENTITY_REPLY = 0x02,
    MSG_PAIR_CONFIRM = 0x03,
    MSG_PAIR_ACK = 0x04,
    MSG_KEEPALIVE = 0x05,
};

struct Packet {
    uint8_t version;
    uint8_t type;
    Identity id;
    uint32_t monotonicMs;
    uint8_t reserved[8];
};
#pragma pack(pop)

// -----------------------------------------------------------------------------
// Simple ESP-NOW discovery helper that implements a controller/controlled
// pairing workflow with identity exchange.
// -----------------------------------------------------------------------------

class EspNowDiscovery {
public:
    void begin();
    void discover();
    bool handleIncoming(const uint8_t *mac, const uint8_t *incomingData, int len);
    bool hasPeers() const;
    int  getPeerCount() const { return peerCount; }
    const uint8_t* getPeer(int index) const;
    const char* getPeerName(int index) const;
    int findPeerIndex(const uint8_t* mac) const;

    // Utility helpers.
    static void macToString(const uint8_t* mac, char* buffer, size_t bufferLen);
    static bool macEqual(const uint8_t* a, const uint8_t* b);

private:
    struct PeerEntry {
        bool inUse = false;
        Identity identity{};
        uint8_t mac[6] = {};
        uint32_t lastSeen = 0;
        bool confirmed = false;
        bool acked = false;
    };

    struct LinkState {
        bool paired = false;
        int peerIndex = -1;
        uint8_t peerMac[6] = {};
        uint32_t lastActivityMs = 0;
        uint32_t lastKeepAliveSentMs = 0;
        uint32_t lastConfirmSentMs = 0;
        bool awaitingAck = false;
    };

    static constexpr int kMaxPeers = 8;
    static constexpr uint8_t kProtocolVersion = 1;

    void fillSelfIdentity();
    bool ensurePeer(const uint8_t* mac) const;
    bool sendPacket(MessageType type, const uint8_t* mac);
    int upsertPeer(const Identity& id, const uint8_t* mac, uint32_t now);
    void pruneExpiredPeers(uint32_t now);
    int selectTarget() const;
    void resetLink();
    int computePeerCount() const;

    Identity selfIdentity{};
    PeerEntry peers[kMaxPeers] = {};
    int peerCount = 0;
    LinkState link{};
    uint32_t lastBroadcastMs = 0;
};

#endif // ESPNOW_DISCOVERY_H
