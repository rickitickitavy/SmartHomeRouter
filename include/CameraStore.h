#pragma once

#include "Defines.h"
#include "PsramJsonBuffer.h"
#include <Arduino.h>
#include <WiFiUdp.h>
#include <stddef.h>
#include <stdint.h>

// ONVIF camera registry.
//
// Memory rule: every CameraRecord lives in PSRAM (heap_caps_malloc SPIRAM|8BIT).
// Strings are fixed-size arrays embedded in those PSRAM blocks, so there is no
// separate string allocation that could land in internal RAM. When PSRAM cannot
// satisfy an allocation the write fails closed with CameraWriteOutOfMemory;
// malloc() is never used for camera data.
//
// Not thread-safe: call from the same task (main loop / web handlers' deferred work).

struct CameraChannel {
    char snapshotUrl[CAMERA_URL_MAX];
    char liveUrl[CAMERA_URL_MAX];
};

struct CameraOnvifSettings {
    char host[CAMERA_HOST_MAX];
    uint16_t port;
    char username[CAMERA_USER_MAX];
    char password[CAMERA_PASSWORD_MAX];
    CameraChannel channels[CAMERA_CHANNELS];
};

struct CameraRecord {
    char id[CAMERA_ID_BUF];
    char name[CAMERA_NAME_MAX];
    bool active;
    uint8_t type;  // CAMERA_TYPE_ONVIF
    char url[CAMERA_URL_MAX];  // primary display URL
    CameraOnvifSettings onvif;

    // Runtime only, never persisted.
    uint8_t status;  // CAMERA_STATUS_*
    char statusError[CAMERA_STATUS_ERROR_MAX];  // short text when status == CAMERA_STATUS_ERROR
    bool probed;
    uint32_t lastProbeMs;

    CameraRecord *next;
};

enum CameraWriteResult {
    CameraWriteOk = 0,
    CameraWriteNotFound,
    CameraWriteFull,
    CameraWriteBadId,
    CameraWriteBadName,
    CameraWriteDuplicateId,
    CameraWriteValidation,
    CameraWriteOutOfMemory,
    CameraWriteParseError
};

struct DiscoveredCamera {
    char name[CAMERA_NAME_MAX];
    char host[CAMERA_HOST_MAX];
    uint16_t port;
    char xaddr[CAMERA_URL_MAX];
    char url[CAMERA_URL_MAX];
    uint8_t type;
    DiscoveredCamera *next;
};

class CameraStore {
public:
    CameraStore();
    ~CameraStore();

    CameraStore(const CameraStore &) = delete;
    CameraStore &operator=(const CameraStore &) = delete;

    bool begin(bool persistToLittleFs);
    void requestPersist();
    void persistIfDue();

    int cameraCount() const;
    CameraRecord *first();
    const CameraRecord *first() const;
    CameraRecord *findById(const char *cameraId);
    const CameraRecord *findById(const char *cameraId) const;

    // Create only on save: the camera exists after the first successful call.
    // "id" is optional in json; when missing a unique "cam-xxxxxxxx" id is generated.
    CameraWriteResult createFromJson(const char *json, CameraRecord **createdOut);
    // Replace an existing camera. keepId=true forces the existing id; keepId=false
    // accepts an id from json (must stay unique). When json has no "password" key the
    // stored password is kept (list/export without secrets does not wipe it).
    CameraWriteResult replaceFromJson(const char *cameraId, const char *json, bool keepId = true);
    CameraWriteResult deleteById(const char *cameraId);
    // Atomic: on any error the current registry is left untouched.
    CameraWriteResult replaceAllFromJson(const char *arrayJson);
    CameraWriteResult replaceAllFromJson(const String &arrayJson);

    // includeSecrets=false -> list shape: id,name,active,type,url,status.
    // includeSecrets=true  -> full shape incl. host,port,username,password,channels.
    // Returns an empty String on allocation failure.
    String listJson(bool includeSecrets) const;
    String cameraFullJson(const CameraRecord *camera, bool includePassword) const;
    static void appendListJson(PsramJsonBuffer &json, const CameraStore &store, bool includeSecrets);
    static void appendCameraFullJson(PsramJsonBuffer &json, const CameraRecord *camera, bool includePassword,
        bool includeStatus = true);
    static void appendCameraListItemJson(PsramJsonBuffer &json, const CameraRecord *camera);

    // Any signed-in user may view camera media (no per-camera user list).
    static bool userMayView(const CameraRecord *camera, const char *userName, bool isAdminOrEditConsoles);

    // Lightweight reachability: TCP connect only (no HTTP body, no image buffer).
    // Blocks up to CAMERA_PROBE_TIMEOUT_MS per camera.
    static void probeStatus(CameraRecord *camera);
    void refreshAllStatus();
    static const char *statusText(const CameraRecord *camera);  // "offline" | "online" | error text

    // ONVIF WS-Discovery only (UDP multicast). Does not resolve snapshot/live URLs.
    // Discovered list is PSRAM-only; Start clears it. tickDiscovery must run on the host loop.
    bool startDiscovery(uint8_t cameraType);
    void stopDiscovery();
    // Stop any in-flight discover and drop the discovered table (open-dialog reset).
    void clearDiscoveryResults();
    bool isDiscovering() const;
    void tickDiscovery();
    // Unknown (not registered by host) discoveries only (host/xaddr; channels filled via Resolve).
    String discoveredJson() const;
    int discoveredCount() const;

    // Resolve ONVIF media URLs from camera dialog host/port/credentials (Resolve button).
    // Writes JSON object: name,url,host,port,channels[{snapshotUrl,liveUrl},...], ok, error.
    String testOnvifJson(const char *jsonBody) const;

    // Test channel snapshot HTTP URLs (Test button). Body: username, password, snapshotUrl1/2
    // (or channels[].snapshotUrl). Fetches each snapshot into PSRAM briefly; never keeps images.
    // Writes {ok, error, channels:[{ok,error,bytes},...]}.
    String testSnapshotsJson(const char *jsonBody) const;

    // Fetch channel snapshot HTTP URL into a new PSRAM buffer (caller frees with free()).
    // Never uses internal RAM for image bytes. Returns nullptr on failure.
    uint8_t *fetchSnapshotJpeg(const char *cameraId, uint8_t channel, size_t *outSize);

    static const char *writeResultText(CameraWriteResult result);
    static bool isIdSafe(const char *cameraId);

    // PSRAM-only allocation helpers (same pattern as AutomationStore::allocateEmailMemory).
    static bool psramAllowsAllocation(size_t bytes);
    static void *allocatePsram(size_t bytes);
    static void freeCamera(CameraRecord *camera);
    static void freeCameraList(CameraRecord *&head);

private:
    CameraRecord *head;
    int storedCount;
    bool persistEnabled;
    bool persistPending;

    DiscoveredCamera *discoveredHead;
    int discoveredStoredCount;
    bool discovering;
    bool discoveryUdpPhase;
    uint8_t discoveryType;
    uint32_t discoveryStartedMs;
    uint32_t discoveryLastProbeMs;
    uint8_t discoveryProbeRound;
    WiFiUDP discoveryUdp;
    bool discoveryUdpOpen;

    bool loadFromFile();
    bool saveToFile() const;
    void clearAll();
    void clearDiscovered();
    CameraRecord *findTail() const;
    void appendRecord(CameraRecord *camera);
    bool isHostRegistered(const char *host) const;
    bool hasDiscoveredHost(const char *host) const;
    void ingestDiscoveryPacket(const char *payload, size_t length);
    bool sendDiscoveryProbe(uint8_t probeKind);
    void sendDiscoveryProbeRound();
    void drainDiscoveryUdp();

    static CameraRecord *allocateRecord();
    static DiscoveredCamera *allocateDiscovered();
    static void freeDiscoveredList(DiscoveredCamera *&listHead);
    static void applyDefaultStatus(CameraRecord *camera);
    static void makeCameraId(char *outId);
    static bool idInList(const CameraRecord *listHead, const char *cameraId, const CameraRecord *skip);
    static CameraWriteResult parseObject(const char *begin, const char *end, const CameraRecord *previous,
        bool requireId, CameraRecord **outCamera);
    static CameraWriteResult fillRecord(CameraRecord *camera, const char *begin, const char *end,
        const CameraRecord *previous, bool requireId);
    static CameraWriteResult parseArray(const char *arrayJson, CameraRecord **outHead, int *outCount);
    static bool parseHttpUrl(const char *url, char *hostOut, size_t hostMax, uint16_t *portOut);
};
