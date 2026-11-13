#include "FirmwareOta.h"
#include "Defines.h"
#include "JoinedFirmwareZip.h"
#include "InterChipHost.h"
#include "InterChipSlave.h"
#include "Logger.h"
#include "StatusRgb.h"

#include <Update.h>
#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <freertos/semphr.h>

#if defined(BOARD_HOST_P4)
#include "esp32-hal-hosted.h"
#endif

FirmwareOta FIRMWARE_OTA;

static portMUX_TYPE gFirmwareOtaMux = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t gPackageFsMutex = nullptr;

static void ensurePackageFsMutex() {
    if (gPackageFsMutex != nullptr) {
        return;
    }
    // Created on loop before Slave SPI transfer; later SPI fail paths only take it.
    gPackageFsMutex = xSemaphoreCreateRecursiveMutex();
}

static void takePackageFs() {
    ensurePackageFsMutex();
    if (gPackageFsMutex != nullptr) {
        xSemaphoreTakeRecursive(gPackageFsMutex, portMAX_DELAY);
    }
}

static void givePackageFs() {
    if (gPackageFsMutex != nullptr) {
        xSemaphoreGiveRecursive(gPackageFsMutex);
    }
}

bool FirmwareOta::busy() const {
    return currentPhase == Phase::Receiving || currentPhase == Phase::Preparing
        || currentPhase == Phase::HostedC6 || currentPhase == Phase::Slave
        || currentPhase == Phase::VerifyingSlave || currentPhase == Phase::Host
        || currentPhase == Phase::Rebooting;
}

bool FirmwareOta::isUpdatingSlave() const {
    return currentPhase == Phase::Slave;
}

bool FirmwareOta::suppressNonOtaSpi() const {
    return currentPhase == Phase::Slave || slaveVerifyResendSettling;
}

bool FirmwareOta::isVerifyingSlave() const {
    return currentPhase == Phase::VerifyingSlave;
}

bool FirmwareOta::isApplyingImage() const {
    return slaveOtaActive || slaveBeginPending || slaveApplyPending || slaveApplyArmed;
}

bool FirmwareOta::slaveBeginAcked() const {
    return beginAcked;
}

bool FirmwareOta::isLastSlaveFrameEnd() const {
    return lastSlavePayloadLength > 0 && (lastSlavePayload[0] & SPI_OTA_END) != 0;
}

FirmwareOta::Phase FirmwareOta::phase() const {
    return currentPhase;
}

const char *FirmwareOta::phaseId() const {
    switch (currentPhase) {
        case Phase::Receiving:
            return "receiving";
        case Phase::Preparing:
            return "preparing";
        case Phase::HostedC6:
            return "hosted_c6";
        case Phase::Slave:
            return "slave";
        case Phase::VerifyingSlave:
            return "verifying_slave";
        case Phase::Host:
            return "host";
        case Phase::Failed:
            return "failed";
        case Phase::Rebooting:
            return "rebooting";
        case Phase::Done:
            return "done";
        case Phase::Idle:
        default:
            return "idle";
    }
}

uint8_t FirmwareOta::percentOf(uint32_t done, uint32_t total) const {
    if (total == 0) {
        return 0;
    }
    if (done >= total) {
        return 100;
    }
    return (uint8_t)((done * 100UL) / total);
}

String FirmwareOta::statusJson() const {
    uint8_t uploadPercent = 0;
    uint8_t slavePercent = 0;
    uint8_t hostPercent = 0;
    if (currentPhase == Phase::Receiving) {
        uploadPercent = percentOf(receivedBytes, packageSize);
    } else if (currentPhase != Phase::Idle && currentPhase != Phase::Failed) {
        uploadPercent = 100;
    }
    if (!hasSlaveImage
        && (currentPhase == Phase::Host || currentPhase == Phase::Rebooting
            || currentPhase == Phase::Done)) {
        slavePercent = 100;
    } else if (currentPhase == Phase::Slave) {
        slavePercent = percentOf(imageOffset, slaveImageSize);
    } else if (
        currentPhase == Phase::VerifyingSlave || currentPhase == Phase::Host
        || currentPhase == Phase::Rebooting || currentPhase == Phase::Done
    ) {
        slavePercent = 100;
    }
    if (!hasHostImage
        && (currentPhase == Phase::VerifyingSlave || currentPhase == Phase::Done
            || (currentPhase == Phase::Slave && hasSlaveImage))) {
        hostPercent = 100;
    } else if (currentPhase == Phase::Host) {
        hostPercent = percentOf(imageOffset, hostImageSize);
    } else if (currentPhase == Phase::Rebooting || currentPhase == Phase::Done) {
        hostPercent = 100;
    }
    if (currentPhase == Phase::Failed && packageSize > 0) {
        uploadPercent = percentOf(receivedBytes > 0 ? receivedBytes : imageOffset, packageSize);
    }
    String json = "{\"phase\":\"";
    json += phaseId();
    json += "\",\"uploadPercent\":";
    json += String((int)uploadPercent);
    json += ",\"slavePercent\":";
    json += String((int)slavePercent);
    json += ",\"hostPercent\":";
    json += String((int)hostPercent);
    json += ",\"bytesDone\":";
    json += String((unsigned long)imageOffset);
    json += ",\"bytesTotal\":";
    json += String((unsigned long)imageSize);
    json += ",\"hasSlave\":";
    json += hasSlaveImage ? "true" : "false";
    json += ",\"hasHost\":";
    json += hasHostImage ? "true" : "false";
    const bool needsReboot = currentPhase == Phase::Rebooting || hostRestartPending
        || (currentPhase == Phase::Done && hasHostImage && hostUpdateStarted);
    json += ",\"needsReboot\":";
    json += needsReboot ? "true" : "false";
    if (currentPhase == Phase::Failed && errorText.length() > 0) {
        json += ",\"error\":\"";
        for (size_t i = 0; i < errorText.length(); i++) {
            const char character = errorText[i];
            if (character == '"' || character == '\\') {
                json += '\\';
            }
            json += character;
        }
        json += "\"";
    }
    json += "}";
    return json;
}

void FirmwareOta::freeSlaveImageRam() {
    if (slaveImageRam != nullptr) {
        free(slaveImageRam);
        slaveImageRam = nullptr;
    }
    slaveImageRamSize = 0;
}

void FirmwareOta::closePackageStream() {
    takePackageFs();
    memberReader.close();
    if (packageFile) {
        packageFile.close();
    }
    givePackageFs();
    freeSlaveImageRam();
}

bool FirmwareOta::loadSlaveImageToRam() {
    freeSlaveImageRam();
    if (slaveImageSize == 0 || !memberReader.isOpen()) {
        return false;
    }
    uint8_t *buffer = static_cast<uint8_t *>(
        heap_caps_malloc(slaveImageSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    );
    if (buffer == nullptr) {
        buffer = static_cast<uint8_t *>(malloc(slaveImageSize));
    }
    if (buffer == nullptr) {
        return false;
    }
    size_t got = 0;
    while (got < slaveImageSize) {
        const size_t n = memberReader.read(buffer + got, slaveImageSize - got);
        if (n == 0 || memberReader.failed()) {
            free(buffer);
            return false;
        }
        got += n;
        yield();
    }
    memberReader.close();
    slaveImageRam = buffer;
    slaveImageRamSize = slaveImageSize;
    return true;
}

void FirmwareOta::setSpiClockRestoreHandler(SpiClockHzFn handler) {
    spiClockRestoreFn = handler;
}

void FirmwareOta::setSpiClockOverrideHandler(SpiClockHzFn handler) {
    spiClockOverrideFn = handler;
}

void FirmwareOta::beginSlaveSpiClockOverride() {
    if (spiClockOverrideActive) {
        return;
    }
    spiClockOverrideActive = true;
    uint32_t overrideHz = DEFAULT_RF_UPDATE_SPI_SPEED_HZ;
    if (spiClockOverrideFn != nullptr) {
        overrideHz = spiClockOverrideFn();
    }
    INTER_CHIP_HOST.setClockHz(overrideHz);
    LOGGER.info(
        "SPI clock temporarily " + String((unsigned long)overrideHz)
        + " Hz for slave firmware OTA"
    );
}

void FirmwareOta::endSlaveSpiClockOverride() {
    if (!spiClockOverrideActive) {
        return;
    }
    spiClockOverrideActive = false;
    uint32_t restoreHz = DEFAULT_SPI_SPEED_HZ;
    if (spiClockRestoreFn != nullptr) {
        restoreHz = spiClockRestoreFn();
    }
    INTER_CHIP_HOST.setClockHz(restoreHz);
    LOGGER.info("SPI clock restored after slave firmware OTA");
}

void FirmwareOta::fail(const char *message) {
    errorText = message != nullptr ? message : "Firmware update failed";
    const bool abortUpdate = hostUpdateStarted || slaveOtaActive;
    currentPhase = Phase::Failed;
    waitingForSlaveResult = false;
    beginQueued = false;
    beginAcked = false;
    hostUpdateStarted = false;
    packagePreparePending = false;
    rebootReadyMs = 0;
    if (stagingFile) {
        stagingFile.close();
    }
    closePackageStream();
    if (abortUpdate) {
        Update.abort();
    }
    slaveOtaActive = false;
    slaveBeginPending = false;
    slaveApplyPending = false;
    slaveApplyArmed = false;
    slaveApplyWaitTransfers = 0;
    slaveBeginExtraLength = 0;
    slaveBytesRemaining = 0;
    lastSlavePayloadLength = 0;
    lastSlaveDataBytes = 0;
    streamPos = 0;
    slaveFrameFails = 0;
    slavePackClaimed = false;
    slaveSpiCycleCount = 0;
    slaveVerifyDeadlineMs = 0;
    slaveVerifyResetPending = false;
    slaveVerifyAwaitingReadyForResend = false;
    slaveVerifyResendSettling = false;
    slaveVerifyResendSettleMs = 0;
    endSlaveSpiClockOverride();
    STATUS_RGB.setUpdateBlinkManual(false);
    clearStagingFiles();
    LOGGER.error(errorText);
}

void FirmwareOta::clearStagingFiles() {
    closePackageStream();
    if (stagingFile) {
        stagingFile.close();
    }
    if (LittleFS.exists(kPackagePath)) {
        LittleFS.remove(kPackagePath);
    }
}

bool FirmwareOta::beginStaging(size_t contentLength) {
    if (busy()) {
        return false;
    }
    errorText = "";
    packageSize = (uint32_t)contentLength;
    slaveImageSize = 0;
    hostImageSize = 0;
    hostedC6ImageSize = 0;
    imageSize = 0;
    imageOffset = 0;
    streamPos = 0;
    receivedBytes = 0;
    waitingForSlaveResult = false;
    beginQueued = false;
    beginAcked = false;
    hostUpdateStarted = false;
    hostRestartPending = false;
    packagePreparePending = false;
    rebootReadyMs = 0;
    hasSlaveImage = false;
    hasHostImage = false;
    hasHostedC6Image = false;
    slaveUpdateRequired = false;
    hostUpdateRequired = false;
    hostedC6UpdateRequired = false;
    expectedSlaveVersion[0] = '\0';
    expectedHostVersion[0] = '\0';
    expectedHostedC6Version[0] = '\0';
    slaveMember = JoinedFirmwareZip::MemberInfo{};
    hostMember = JoinedFirmwareZip::MemberInfo{};
    hostedC6Member = JoinedFirmwareZip::MemberInfo{};
    LittleFS.mkdir("/ota");
    if (contentLength > 0) {
        const size_t freeBytes = LittleFS.totalBytes() - LittleFS.usedBytes();
        if (freeBytes < contentLength + 4096) {
            fail("Not enough filesystem space for firmware");
            return false;
        }
    }
    clearStagingFiles();
    stagingFile = LittleFS.open(kPackagePath, "w");
    if (!stagingFile) {
        fail("Could not create firmware staging file");
        return false;
    }
    currentPhase = Phase::Receiving;
    LOGGER.info("Firmware staging start (joined ZIP)");
    return true;
}

bool FirmwareOta::writeStaging(const uint8_t *data, size_t length) {
    if (currentPhase != Phase::Receiving || !stagingFile) {
        return false;
    }
    if (length == 0) {
        return true;
    }
    if (data == nullptr) {
        fail("Firmware staging write failed");
        return false;
    }
    if (stagingFile.write(data, length) != length) {
        fail("Firmware staging write failed");
        return false;
    }
    receivedBytes += (uint32_t)length;
    return true;
}

#if defined(BOARD_HOST_P4)
bool FirmwareOta::readRunningHostedC6Version(char *destination, size_t destinationSize) {
    if (destination == nullptr || destinationSize == 0) {
        return false;
    }
    destination[0] = '\0';
    if (!hostedIsInitialized()) {
        return false;
    }
    // One-shot refresh of the Hosted version cache (not used on ordinary Status polls).
    (void)hostedHasUpdate();
    uint32_t major = 0;
    uint32_t minor = 0;
    uint32_t patch = 0;
    hostedGetSlaveVersion(&major, &minor, &patch);
    if (major == 0 && minor == 0 && patch == 0) {
        return false;
    }
    snprintf(
        destination,
        destinationSize,
        "%lu.%lu.%lu",
        (unsigned long)major,
        (unsigned long)minor,
        (unsigned long)patch
    );
    return destination[0] != '\0';
}
#endif

bool FirmwareOta::prepareJoinedPackage() {
    ensurePackageFsMutex();
    closePackageStream();
    takePackageFs();
    packageFile = LittleFS.open(kPackagePath, "r");
    if (!packageFile) {
        givePackageFs();
        fail("Joined package missing");
        return false;
    }
#if defined(BOARD_HOST_P4)
    if (!JoinedFirmwareZip::findMembers(packageFile, &slaveMember, &hostMember, &hostedC6Member)) {
#else
    if (!JoinedFirmwareZip::findMembers(packageFile, &slaveMember, &hostMember)) {
#endif
        givePackageFs();
        closePackageStream();
        fail("Joined package needs slave.bin and/or host.bin");
        return false;
    }
    hasSlaveImage = slaveMember.found;
    hasHostImage = hostMember.found;
#if defined(BOARD_HOST_P4)
    hasHostedC6Image = hostedC6Member.found;
#else
    hasHostedC6Image = false;
    hostedC6Member = JoinedFirmwareZip::MemberInfo{};
#endif
    expectedSlaveVersion[0] = '\0';
    expectedHostVersion[0] = '\0';
    expectedHostedC6Version[0] = '\0';
    slaveUpdateRequired = hasSlaveImage;
    hostUpdateRequired = hasHostImage;
    hostedC6UpdateRequired = false;
    slaveImageSize = hasSlaveImage ? slaveMember.uncompressedSize : 0;
    hostImageSize = hasHostImage ? hostMember.uncompressedSize : 0;
    hostedC6ImageSize = hasHostedC6Image ? hostedC6Member.uncompressedSize : 0;
    imageOffset = 0;
    streamPos = 0;
    if (hasSlaveImage) {
        if (!JoinedFirmwareZip::readSlaveVersionText(
                packageFile,
                expectedSlaveVersion,
                sizeof(expectedSlaveVersion)
            )) {
            LOGGER.warning(
                "Joined package has no slave.version.txt/version.txt; will require slave version change after OTA"
            );
        }
    }
    if (hasHostImage) {
        if (!JoinedFirmwareZip::readHostVersionText(
                packageFile,
                expectedHostVersion,
                sizeof(expectedHostVersion)
            )) {
            LOGGER.warning("Joined package has no host.version.txt");
        }
    }
#if defined(BOARD_HOST_P4)
    if (hasHostedC6Image) {
        if (!JoinedFirmwareZip::readHostedC6VersionText(
                packageFile,
                expectedHostedC6Version,
                sizeof(expectedHostedC6Version)
            )
            || expectedHostedC6Version[0] == '\0') {
            givePackageFs();
            closePackageStream();
            fail("hosted-c6.bin present but hosted-c6.version.txt missing");
            return false;
        }
        char runningHosted[SPI_STATUS_VERSION_MAX + 1]{};
        if (!readRunningHostedC6Version(runningHosted, sizeof(runningHosted))) {
            givePackageFs();
            closePackageStream();
            fail("Hosted C6 version unreadable");
            return false;
        }
        if (strcmp(runningHosted, expectedHostedC6Version) == 0) {
            hostedC6UpdateRequired = false;
            LOGGER.info(
                "Hosted C6 already at package version "
                + String(expectedHostedC6Version) + "; skipping SDIO update"
            );
        } else {
            hostedC6UpdateRequired = true;
            LOGGER.info(
                "Hosted C6 update required running="
                + String(runningHosted) + " package=" + String(expectedHostedC6Version)
            );
        }
    }
#endif
    if (hasSlaveImage && slaveVersionAlreadyCurrent()) {
        slaveUpdateRequired = false;
        LOGGER.info(
            "Slave already at package version "
            + String(expectedSlaveVersion) + "; skipping SPI transfer"
        );
    }
    if (hasHostImage && hostVersionAlreadyCurrent()) {
        hostUpdateRequired = false;
        LOGGER.info(
            "Host already at package version "
            + String(expectedHostVersion) + "; skipping host apply"
        );
    }
    // Prefetch slave only when Hosted C6 will not run first (memberReader reused).
    if (slaveUpdateRequired && !hostedC6UpdateRequired) {
        if (!memberReader.open(packageFile, slaveMember)) {
            givePackageFs();
            closePackageStream();
            fail("Failed to open slave.bin stream");
            return false;
        }
        imageSize = slaveImageSize;
        if (!loadSlaveImageToRam()) {
            givePackageFs();
            closePackageStream();
            fail("Failed to load slave.bin into RAM");
            return false;
        }
    } else if (!hostedC6UpdateRequired) {
        imageSize = hostUpdateRequired ? hostImageSize : 0;
    } else {
        imageSize = hostedC6ImageSize;
    }
    givePackageFs();
    LOGGER.info(
        "Joined package OK hostedC6="
        + (hostedC6UpdateRequired ? String((unsigned long)hostedC6ImageSize) : String("skip"))
        + " slave="
        + (slaveUpdateRequired ? String((unsigned long)slaveImageSize) : String("skip"))
        + " host="
        + (hostUpdateRequired ? String((unsigned long)hostImageSize) : String("skip"))
        + " expectHostedC6="
        + (expectedHostedC6Version[0] != '\0' ? expectedHostedC6Version : "(none)")
        + " expectSlave="
        + (expectedSlaveVersion[0] != '\0' ? expectedSlaveVersion : "(any-new)")
        + " expectHost="
        + (expectedHostVersion[0] != '\0' ? expectedHostVersion : "(none)")
    );
    return true;
}

bool FirmwareOta::finishStaging() {
    if (currentPhase != Phase::Receiving || !stagingFile) {
        return false;
    }
    stagingFile.close();
    stagingFile = File();
    packageSize = 0;
    {
        File sized = LittleFS.open(kPackagePath, "r");
        if (!sized) {
            fail("Firmware staging file missing");
            return false;
        }
        packageSize = (uint32_t)sized.size();
        sized.close();
    }
    if (packageSize == 0) {
        fail("Firmware image is empty");
        return false;
    }
    lastSlavePayloadLength = 0;
    lastSlaveDataBytes = 0;
    slaveFrameFails = 0;
    packagePreparePending = true;
    currentPhase = Phase::Preparing;
    LOGGER.info("Firmware staging done, preparing joined package stream");
    return true;
}

void FirmwareOta::abortStaging() {
    if (stagingFile) {
        stagingFile.close();
    }
    if (currentPhase == Phase::Receiving || currentPhase == Phase::Preparing) {
        fail("Firmware upload aborted");
    }
}

bool FirmwareOta::sendLastSlaveFrame(bool isResend) {
    uint8_t localPayload[SPI_MAX_PAYLOAD];
    uint16_t localLength = 0;
    uint8_t localSeq = 0;
    bool beginWasQueued = false;
    bool beginWasAcked = false;
    uint8_t frameFails = 0;
    uint32_t offsetSnapshot = 0;

    portENTER_CRITICAL(&gFirmwareOtaMux);
    beginWasQueued = beginQueued;
    beginWasAcked = beginAcked;
    frameFails = slaveFrameFails;
    offsetSnapshot = imageOffset;
    if (lastSlavePayloadLength == 0) {
        portEXIT_CRITICAL(&gFirmwareOtaMux);
        // Never enqueue a zero-length OTA frame. Before begin is staged this is
        // expected under loop/hostSpi races — let the next pump pack begin.
        if (!beginWasQueued) {
            LOGGER.warning(
                "SPI firmware OTA resend skipped; begin not staged yet "
                "fails=" + String((int)frameFails) + " offset=" + String((unsigned long)offsetSnapshot)
            );
            return false;
        }
        LOGGER.warning(
            "SPI firmware OTA last frame empty beginQueued=1 beginAcked="
            + String(beginWasAcked ? 1 : 0) + " fails=" + String((int)frameFails)
            + " offset=" + String((unsigned long)offsetSnapshot)
        );
        return false;
    }
    // Already waiting on an in-flight frame: do not enqueue a newer seq ahead of it
    // (that produced ignore result seq=N expect=N+1 and stuck RF at 0%).
    if (waitingForSlaveResult && !isResend) {
        portEXIT_CRITICAL(&gFirmwareOtaMux);
        return true;
    }
    localLength = lastSlavePayloadLength;
    memcpy(localPayload, lastSlavePayload, localLength);
    localSeq = lastSlaveSeq;
    // Claim in-flight before tryEnqueue so pump/hostSpi cannot race a second seq.
    waitingForSlaveResult = true;
    portEXIT_CRITICAL(&gFirmwareOtaMux);

    bool queued = false;
    if (isResend && localSeq != 0) {
        queued = INTER_CHIP_HOST.tryEnqueueWithSeq(
            SpiCmdFirmwareOta,
            localSeq,
            localPayload,
            localLength
        );
    } else {
        queued = INTER_CHIP_HOST.tryEnqueue(SpiCmdFirmwareOta, localPayload, localLength);
        portENTER_CRITICAL(&gFirmwareOtaMux);
        lastSlaveSeq = INTER_CHIP_HOST.lastEnqueuedSeq();
        portEXIT_CRITICAL(&gFirmwareOtaMux);
    }
    if (!queued) {
        portENTER_CRITICAL(&gFirmwareOtaMux);
        waitingForSlaveResult = false;
        portEXIT_CRITICAL(&gFirmwareOtaMux);
        LOGGER.warning("SPI firmware frame queue full, will retry");
        return false;
    }
    return true;
}

void FirmwareOta::onSlaveFrameLost(bool fromNack) {
    uint8_t frameFails = 0;
    uint16_t payloadLength = 0;
    bool beginWasQueued = false;
    bool beginWasAcked = false;
    bool imageComplete = false;
    uint8_t lostSeq = 0;
    uint32_t offsetSnapshot = 0;

    bool lostEndFrame = false;
    portENTER_CRITICAL(&gFirmwareOtaMux);
    waitingForSlaveResult = false;
    if (slaveFrameFails < 255) {
        slaveFrameFails++;
    }
    frameFails = slaveFrameFails;
    payloadLength = lastSlavePayloadLength;
    beginWasQueued = beginQueued;
    beginWasAcked = beginAcked;
    lostSeq = lastSlaveSeq;
    offsetSnapshot = imageOffset;
    lostEndFrame = payloadLength > 0 && (lastSlavePayload[0] & SPI_OTA_END) != 0;
    // Count only ACKed bytes plus the in-flight END payload; require END flag for escape.
    imageComplete = lostEndFrame && (imageOffset + lastSlaveDataBytes >= slaveImageSize);
    portEXIT_CRITICAL(&gFirmwareOtaMux);

    LOGGER.warning(
        String(fromNack ? "SPI firmware frame nack seq=" : "SPI firmware frame lost seq=")
        + String((int)lostSeq) + " fail=" + String((int)frameFails) + "/"
        + String((int)kMaxSlaveFrameFails)
    );
    if (frameFails >= kMaxSlaveFrameFails) {
        // Only a quiet TIMEOUT on END (not NACK) may mean slave is applying and ACK was dropped.
        // END NACKs usually mean byte-count mismatch / premature END — never pretend success.
        if (!fromNack && beginWasAcked && imageComplete) {
            portENTER_CRITICAL(&gFirmwareOtaMux);
            imageOffset = slaveImageSize;
            lastSlavePayloadLength = 0;
            lastSlaveDataBytes = 0;
            portEXIT_CRITICAL(&gFirmwareOtaMux);
            INTER_CHIP_HOST.noteKeepaliveQuiet();
            LOGGER.info("Slave END ACK lost after complete image; verifying slave version");
            beginSlaveVersionVerify();
            return;
        }
        if (fromNack && lostEndFrame) {
            fail("Slave rejected firmware OTA END (image incomplete or apply not armed)");
            return;
        }
        fail("Slave firmware update failed after repeated SPI frame loss");
        return;
    }
    if (payloadLength == 0) {
        if (!beginWasQueued) {
            LOGGER.warning(
                "SPI firmware OTA empty buffer before begin staged; waiting to pack begin "
                "offset=" + String((unsigned long)offsetSnapshot)
            );
            return;
        }
        if (!beginWasAcked) {
            // Begin was marked queued but buffer was cleared without ACK — re-pack begin.
            portENTER_CRITICAL(&gFirmwareOtaMux);
            beginQueued = false;
            streamPos = 0;
            imageOffset = 0;
            portEXIT_CRITICAL(&gFirmwareOtaMux);
            LOGGER.warning(
                "SPI firmware OTA empty buffer before begin ACK; will re-pack begin "
                "fails=" + String((int)frameFails)
            );
            return;
        }
        // Buffer empty after begin ACK: never invent bytes and never advance the
        // stream past what the slave ACKed (streamPos can race ahead of imageOffset).
        portENTER_CRITICAL(&gFirmwareOtaMux);
        streamPos = imageOffset;
        lastSlavePayloadLength = 0;
        lastSlaveDataBytes = 0;
        portEXIT_CRITICAL(&gFirmwareOtaMux);
        LOGGER.warning(
            "SPI firmware OTA empty buffer after begin ACK; resync stream to offset="
            + String((unsigned long)offsetSnapshot) + " fails=" + String((int)frameFails)
        );
        return;
    }
    sendLastSlaveFrame(true);
}

bool FirmwareOta::packSlaveChunkFromStream(
    uint8_t *payloadOut,
    uint16_t *packedLengthOut,
    uint32_t *dataBytesOut
) {
    if (payloadOut == nullptr || packedLengthOut == nullptr || dataBytesOut == nullptr) {
        return false;
    }
    if (slaveImageRam == nullptr || slaveImageRamSize == 0 || slaveImageRamSize != slaveImageSize) {
        fail("Slave image RAM missing");
        return false;
    }
    if (streamPos >= slaveImageSize) {
        *packedLengthOut = 0;
        *dataBytesOut = 0;
        return true;
    }
    uint8_t chunk[SPI_FILE_CHUNK_MAX];
    const uint32_t remaining = slaveImageSize - streamPos;
    const size_t toRead = remaining > SPI_FILE_CHUNK_MAX ? SPI_FILE_CHUNK_MAX : (size_t)remaining;
    memcpy(chunk, slaveImageRam + streamPos, toRead);
    uint8_t flags = 0;
    if (streamPos + (uint32_t)toRead >= slaveImageSize) {
        flags = SPI_OTA_END;
    }
    const uint16_t packedLength =
        spiPackFirmwareOta(payloadOut, SPI_MAX_PAYLOAD, flags, chunk, (uint16_t)toRead);
    if (packedLength == 0) {
        fail("Firmware OTA pack failed");
        return false;
    }
    streamPos += (uint32_t)toRead;
    *packedLengthOut = packedLength;
    *dataBytesOut = (uint32_t)toRead;
    return true;
}

bool FirmwareOta::enqueueNextSlaveChunk() {
    if (!INTER_CHIP_HOST.isNormal()) {
        fail("Slave link is not ready");
        return false;
    }

    portENTER_CRITICAL(&gFirmwareOtaMux);
    if (waitingForSlaveResult || slavePackClaimed) {
        portEXIT_CRITICAL(&gFirmwareOtaMux);
        return true;
    }
    if (lastSlavePayloadLength > 0) {
        const bool isResend = slaveFrameFails > 0;
        portEXIT_CRITICAL(&gFirmwareOtaMux);
        // sendLastSlaveFrame re-checks length under the same mux (loop vs hostSpi TOCTOU).
        return sendLastSlaveFrame(isResend);
    }
    slavePackClaimed = true;
    const bool doBegin = !beginQueued;
    if (doBegin) {
        beginQueued = true;
    }
    portEXIT_CRITICAL(&gFirmwareOtaMux);

    uint8_t payload[SPI_MAX_PAYLOAD];
    uint16_t packedLength = 0;
    uint32_t dataBytes = 0;
    if (doBegin) {
        uint8_t sizeBytes[4];
        sizeBytes[0] = (uint8_t)(slaveImageSize & 0xFF);
        sizeBytes[1] = (uint8_t)((slaveImageSize >> 8) & 0xFF);
        sizeBytes[2] = (uint8_t)((slaveImageSize >> 16) & 0xFF);
        sizeBytes[3] = (uint8_t)((slaveImageSize >> 24) & 0xFF);
        packedLength = spiPackFirmwareOta(payload, sizeof(payload), SPI_OTA_BEGIN, sizeBytes, 4);
        LOGGER.info("Slave firmware begin queued; waiting for erase");
        if (packedLength == 0) {
            portENTER_CRITICAL(&gFirmwareOtaMux);
            slavePackClaimed = false;
            beginQueued = false;
            portEXIT_CRITICAL(&gFirmwareOtaMux);
            fail("Firmware OTA pack failed");
            return false;
        }
    } else if (!packSlaveChunkFromStream(payload, &packedLength, &dataBytes)) {
        portENTER_CRITICAL(&gFirmwareOtaMux);
        slavePackClaimed = false;
        portEXIT_CRITICAL(&gFirmwareOtaMux);
        return false;
    }

    if (packedLength == 0) {
        bool imageComplete = false;
        portENTER_CRITICAL(&gFirmwareOtaMux);
        slavePackClaimed = false;
        imageComplete = imageOffset >= slaveImageSize;
        if (!imageComplete) {
            // Pack cursor ran past ACKed bytes — resync and pack again; do not verify yet.
            streamPos = imageOffset;
            portEXIT_CRITICAL(&gFirmwareOtaMux);
            LOGGER.warning(
                "SPI firmware OTA pack exhausted before ACK complete; resync streamPos="
                + String((unsigned long)imageOffset) + "/" + String((unsigned long)slaveImageSize)
            );
            return true;
        }
        portEXIT_CRITICAL(&gFirmwareOtaMux);
        beginSlaveVersionVerify();
        return true;
    }

    portENTER_CRITICAL(&gFirmwareOtaMux);
    memcpy(lastSlavePayload, payload, packedLength);
    lastSlavePayloadLength = packedLength;
    lastSlaveDataBytes = dataBytes;
    slavePackClaimed = false;
    portEXIT_CRITICAL(&gFirmwareOtaMux);
    return sendLastSlaveFrame(false);
}

void FirmwareOta::onSpiFrame(const SpiFrame &frame) {
    if (currentPhase != Phase::Slave) {
        return;
    }
    if (frame.cmd == SpiEvtTimeout) {
        if (frame.length < 1 || frame.payload[0] != SpiCmdFirmwareOta) {
            return;
        }
        portENTER_CRITICAL(&gFirmwareOtaMux);
        const bool waiting = waitingForSlaveResult;
        const uint8_t expectSeq = lastSlaveSeq;
        portEXIT_CRITICAL(&gFirmwareOtaMux);
        if (!waiting) {
            return;
        }
        // Ignore stale timeouts for a previous OTA seq after we already moved on.
        if (expectSeq != 0 && frame.seq != 0 && frame.seq != expectSeq) {
            return;
        }
        onSlaveFrameLost(false);
        return;
    }
    if (frame.cmd != SpiEvtCmdResult || frame.length < 1) {
        return;
    }
    portENTER_CRITICAL(&gFirmwareOtaMux);
    if (!waitingForSlaveResult) {
        portEXIT_CRITICAL(&gFirmwareOtaMux);
        return;
    }
    // Late/duplicate ACKs for an older seq must not advance imageOffset (premature END).
    if (lastSlaveSeq != 0 && frame.seq != lastSlaveSeq) {
        portEXIT_CRITICAL(&gFirmwareOtaMux);
        LOGGER.warning(
            "SPI firmware OTA ignore result seq=" + String((int)frame.seq) + " expect="
            + String((int)lastSlaveSeq)
        );
        return;
    }
    waitingForSlaveResult = false;
    if (frame.payload[0] == 0) {
        portEXIT_CRITICAL(&gFirmwareOtaMux);
        onSlaveFrameLost(true);
        return;
    }
    slaveFrameFails = 0;
    if (!beginAcked) {
        beginAcked = true;
        lastSlavePayloadLength = 0;
        lastSlaveDataBytes = 0;
        portEXIT_CRITICAL(&gFirmwareOtaMux);
        LOGGER.info("Slave firmware begin acked, sending image");
        enqueueNextSlaveChunk();
        return;
    }
    imageOffset += lastSlaveDataBytes;
    lastSlavePayloadLength = 0;
    lastSlaveDataBytes = 0;
    const bool done = imageOffset >= slaveImageSize;
    portEXIT_CRITICAL(&gFirmwareOtaMux);
    if (done) {
        beginSlaveVersionVerify();
        return;
    }
    enqueueNextSlaveChunk();
}

bool FirmwareOta::slaveVersionSampleAvailable() const {
    if (!INTER_CHIP_HOST.isNormal()) {
        return false;
    }
    const char *seen = INTER_CHIP_HOST.slaveFirmwareVersion();
    return seen != nullptr && seen[0] != '\0';
}

bool FirmwareOta::slaveVersionIsPreOta() const {
    if (!slaveVersionSampleAvailable() || preOtaSlaveVersion[0] == '\0') {
        return false;
    }
    return strcmp(INTER_CHIP_HOST.slaveFirmwareVersion(), preOtaSlaveVersion) == 0;
}

bool FirmwareOta::slaveVersionMatchesExpected() const {
    if (!slaveVersionSampleAvailable()) {
        return false;
    }
    const char *seen = INTER_CHIP_HOST.slaveFirmwareVersion();
    if (expectedSlaveVersion[0] != '\0') {
        return strcmp(seen, expectedSlaveVersion) == 0;
    }
    if (preOtaSlaveVersion[0] == '\0') {
        return false;
    }
    return strcmp(seen, preOtaSlaveVersion) != 0;
}

void FirmwareOta::startSlaveUploadCycle() {
    waitingForSlaveResult = false;
    beginQueued = false;
    beginAcked = false;
    imageSize = slaveImageSize;
    imageOffset = 0;
    streamPos = 0;
    lastSlavePayloadLength = 0;
    lastSlaveDataBytes = 0;
    lastSlaveSeq = 0;
    slaveFrameFails = 0;
    slavePackClaimed = false;
    slaveVerifyResetPending = false;
    slaveVerifyAwaitingReadyForResend = false;
    slaveVerifyResendSettling = false;
    slaveVerifyResendSettleMs = 0;
    if (slaveSpiCycleCount < 255) {
        slaveSpiCycleCount++;
    }
    currentPhase = Phase::Slave;
    beginSlaveSpiClockOverride();
    STATUS_RGB.setUpdateBlinkManual(true);
    INTER_CHIP_HOST.holdForFirmwareOta();
    INTER_CHIP_HOST.noteKeepaliveQuiet();
    LOGGER.info(
        "Slave SPI transfer cycle "
        + String((int)slaveSpiCycleCount) + "/" + String((int)kSlaveVerifyMaxCycles)
    );
}

void FirmwareOta::beginSlaveVersionVerify() {
    endSlaveSpiClockOverride();
    STATUS_RGB.setUpdateBlinkManual(false);
    waitingForSlaveResult = false;
    lastSlavePayloadLength = 0;
    lastSlaveDataBytes = 0;
    slaveVerifyResetPending = false;
    slaveVerifyAwaitingReadyForResend = false;
    slaveVerifyResendSettling = false;
    slaveVerifyResendSettleMs = 0;
    INTER_CHIP_HOST.clearSlaveFirmwareVersion();
    slaveVerifyDeadlineMs = millis() + kSlaveVerifyWaitMs;
    currentPhase = Phase::VerifyingSlave;
    INTER_CHIP_HOST.noteKeepaliveQuiet();
    LOGGER.info(
        "Slave image transferred; waiting up to 20s for version "
        + String(expectedSlaveVersion[0] != '\0' ? expectedSlaveVersion : "(changed)")
        + " (SPI cycle "
        + String((int)slaveSpiCycleCount) + "/" + String((int)kSlaveVerifyMaxCycles) + ")"
    );
}

void FirmwareOta::pumpSlaveVersionVerify() {
    if (currentPhase != Phase::VerifyingSlave) {
        return;
    }
    if (slaveVerifyResetPending) {
        slaveVerifyResetPending = false;
        INTER_CHIP_HOST.clearSlaveFirmwareVersion();
        INTER_CHIP_HOST.resetSlaveSynchronous();
        slaveVerifyAwaitingReadyForResend = true;
        slaveVerifyResendSettling = false;
        slaveVerifyResendSettleMs = 0;
        LOGGER.info(
            "Slave reset after version verify fail; waiting ready then full SPI resend "
            + String((int)slaveSpiCycleCount) + "/" + String((int)kSlaveVerifyMaxCycles)
        );
        return;
    }
    if (slaveVerifyAwaitingReadyForResend) {
        if (!INTER_CHIP_HOST.isNormal()) {
            return;
        }
        // After settings/store dump, wait for a quiet SPI link before OTA cycle 2+
        // (avoids host task WDT from OTA+MQTT+registry flood).
        if (!slaveVerifyResendSettling) {
            slaveVerifyResendSettling = true;
            slaveVerifyResendSettleMs = millis() + kSlaveVerifyResendSettleMs;
            INTER_CHIP_HOST.holdForFirmwareOta();
            LOGGER.info(
                "Slave ready after verify fail; settling SPI "
                + String((unsigned long)kSlaveVerifyResendSettleMs) + "ms before resend "
                + String((int)(slaveSpiCycleCount + 1)) + "/" + String((int)kSlaveVerifyMaxCycles)
            );
            return;
        }
        INTER_CHIP_HOST.holdForFirmwareOta();
        if ((long)(millis() - slaveVerifyResendSettleMs) < 0) {
            return;
        }
        if (!INTER_CHIP_HOST.isOutboundIdle()) {
            return;
        }
        slaveVerifyAwaitingReadyForResend = false;
        slaveVerifyResendSettling = false;
        slaveVerifyResendSettleMs = 0;
        LOGGER.info(
            "Slave SPI quiet after verify fail; starting full SPI transfer "
            + String((int)(slaveSpiCycleCount + 1)) + "/" + String((int)kSlaveVerifyMaxCycles)
        );
        startSlaveUploadCycle();
        return;
    }
    if (slaveVersionMatchesExpected()) {
        LOGGER.info(
            "Slave version verified: " + String(INTER_CHIP_HOST.slaveFirmwareVersion())
        );
        if (hostUpdateRequired) {
            startHostApply();
        } else if (hasHostImage) {
            finishNoOpSuccess("slave updated; host already at package version");
        } else {
            finishSlaveOnlySuccess();
        }
        return;
    }
    // Keep waiting through the full window (including pre-OTA / empty samples).
    if ((long)(millis() - slaveVerifyDeadlineMs) < 0) {
        return;
    }
    // Deadline without expected version: reboot → wait ready → full SPI cycle (max 3).
    if (slaveSpiCycleCount >= kSlaveVerifyMaxCycles) {
        fail("Slave did not report expected firmware version after transfer");
        return;
    }
    const String seen =
        slaveVersionSampleAvailable()
            ? String(INTER_CHIP_HOST.slaveFirmwareVersion())
            : String("(none)");
    LOGGER.warning(
        "Slave version verify failed after 20s (seen=\"" + seen
        + "\"); will reboot and full SPI resend "
        + String((int)(slaveSpiCycleCount + 1)) + "/" + String((int)kSlaveVerifyMaxCycles)
    );
    slaveVerifyResetPending = true;
}

void FirmwareOta::finishSlaveOnlySuccess() {
    endSlaveSpiClockOverride();
    STATUS_RGB.setUpdateBlinkManual(false);
    freeSlaveImageRam();
    closePackageStream();
    clearStagingFiles();
    currentPhase = Phase::Done;
    LOGGER.info("Slave-only firmware update complete (host unchanged)");
}

bool FirmwareOta::slaveVersionAlreadyCurrent() const {
    if (expectedSlaveVersion[0] == '\0') {
        return false;
    }
    const char *running = INTER_CHIP_HOST.slaveFirmwareVersion();
    return running != nullptr && running[0] != '\0' && strcmp(running, expectedSlaveVersion) == 0;
}

bool FirmwareOta::hostVersionAlreadyCurrent() const {
    if (expectedHostVersion[0] == '\0') {
        return false;
    }
    return strcmp(FIRMWARE_VERSION_HOST, expectedHostVersion) == 0;
}

void FirmwareOta::finishNoOpSuccess(const char *reason) {
    endSlaveSpiClockOverride();
    STATUS_RGB.setUpdateBlinkManual(false);
    freeSlaveImageRam();
    closePackageStream();
    clearStagingFiles();
    currentPhase = Phase::Done;
    LOGGER.info(
        String("Firmware update no-op: ") + (reason != nullptr ? reason : "already current")
    );
}

bool FirmwareOta::startHostApply() {
    endSlaveSpiClockOverride();
    STATUS_RGB.setUpdateBlinkManual(false);
    freeSlaveImageRam();
    if (!hasHostImage) {
        fail("Joined package has no host.bin");
        return false;
    }
    if (!hostUpdateRequired || hostVersionAlreadyCurrent()) {
        finishNoOpSuccess("host already at package version");
        return true;
    }
    if (!packageFile) {
        packageFile = LittleFS.open(kPackagePath, "r");
        if (!packageFile) {
            fail("Joined package missing for host stream");
            return false;
        }
        if (!JoinedFirmwareZip::findMembers(packageFile, &slaveMember, &hostMember)
            || !hostMember.found) {
            closePackageStream();
            fail("Joined package lost host.bin");
            return false;
        }
        hasHostImage = hostMember.found;
        hasSlaveImage = slaveMember.found;
    }
    memberReader.close();
    if (!memberReader.open(packageFile, hostMember)) {
        closePackageStream();
        fail("Failed to open host.bin stream");
        return false;
    }
    hostImageSize = hostMember.uncompressedSize;
    if (hostImageSize == 0) {
        closePackageStream();
        fail("Host image is empty");
        return false;
    }
    Update.abort();
    if (!Update.begin(hostImageSize, U_FLASH)) {
        closePackageStream();
        fail("Host OTA begin failed");
        return false;
    }
    hostUpdateStarted = true;
    currentPhase = Phase::Host;
    imageSize = hostImageSize;
    imageOffset = 0;
    INTER_CHIP_HOST.noteKeepaliveQuiet();
    LOGGER.info(
        hasSlaveImage ? "Slave firmware committed, streaming host update"
                      : "Host-only package, streaming host update"
    );
    return true;
}

void FirmwareOta::pumpHostApply() {
    if (!hostUpdateStarted || !memberReader.isOpen()) {
        return;
    }
    uint8_t chunk[1024];
    const size_t remaining = hostImageSize - imageOffset;
    const size_t toRead = remaining > sizeof(chunk) ? sizeof(chunk) : remaining;
    const size_t readLength = memberReader.read(chunk, toRead);
    if (readLength != toRead || memberReader.failed()) {
        fail("Host OTA stream read failed");
        return;
    }
    if (Update.write(chunk, readLength) != readLength) {
        fail("Host OTA write failed");
        return;
    }
    imageOffset += (uint32_t)readLength;
    if (imageOffset < hostImageSize) {
        return;
    }
    if (!Update.end(true)) {
        fail("Host OTA end failed");
        return;
    }
    clearStagingFiles();
    currentPhase = Phase::Rebooting;
    rebootReadyMs = millis() + 2500;
    LOGGER.info("Host firmware written; reboot wait");
}

void FirmwareOta::pumpSlaveBegin() {
    if (!slaveBeginPending) {
        return;
    }
    slaveBeginPending = false;
    LOGGER.info("Slave firmware erase start size=" + String((unsigned long)slaveBytesRemaining));
    if (!Update.begin(slaveBytesRemaining, U_FLASH)) {
        slaveOtaActive = false;
        slaveBytesRemaining = 0;
        slaveBeginExtraLength = 0;
        LOGGER.error("Slave firmware erase failed");
        INTER_CHIP_SLAVE.completeCommandResult(slaveBeginSeq, false);
        return;
    }
    bool writeOk = true;
    if (slaveBeginExtraLength > 0) {
        if (slaveBeginExtraLength > slaveBytesRemaining
            || Update.write(slaveBeginExtra, slaveBeginExtraLength) != slaveBeginExtraLength) {
            writeOk = false;
        } else {
            slaveBytesRemaining -= slaveBeginExtraLength;
        }
        slaveBeginExtraLength = 0;
    }
    if (!writeOk) {
        Update.abort();
        slaveOtaActive = false;
        slaveBytesRemaining = 0;
        LOGGER.error("Slave firmware first chunk write failed");
        INTER_CHIP_SLAVE.completeCommandResult(slaveBeginSeq, false);
        return;
    }
    LOGGER.info("Slave firmware erase done");
    INTER_CHIP_SLAVE.completeCommandResult(slaveBeginSeq, true);
}

void FirmwareOta::continueAfterHostedC6() {
    if (!slaveUpdateRequired && !hostUpdateRequired) {
        finishNoOpSuccess(
            hasHostedC6Image ? "Hosted C6 done; host and slave already match package versions"
                             : "host and slave already match package versions"
        );
        return;
    }
    if (slaveUpdateRequired) {
        ensurePackageFsMutex();
        takePackageFs();
        if (!packageFile) {
            packageFile = LittleFS.open(kPackagePath, "r");
        }
        if (!packageFile
            || !JoinedFirmwareZip::findMembers(
                   packageFile, &slaveMember, &hostMember, &hostedC6Member
               )
            || !slaveMember.found) {
            givePackageFs();
            fail("Joined package lost slave.bin after Hosted C6");
            return;
        }
        hasSlaveImage = slaveMember.found;
        hasHostImage = hostMember.found;
        memberReader.close();
        if (!memberReader.open(packageFile, slaveMember)) {
            givePackageFs();
            fail("Failed to open slave.bin stream after Hosted C6");
            return;
        }
        imageSize = slaveImageSize;
        imageOffset = 0;
        streamPos = 0;
        if (!loadSlaveImageToRam()) {
            givePackageFs();
            fail("Failed to load slave.bin into RAM after Hosted C6");
            return;
        }
        givePackageFs();
        strncpy(
            preOtaSlaveVersion,
            INTER_CHIP_HOST.slaveFirmwareVersion(),
            sizeof(preOtaSlaveVersion) - 1
        );
        preOtaSlaveVersion[sizeof(preOtaSlaveVersion) - 1] = '\0';
        slaveSpiCycleCount = 0;
        startSlaveUploadCycle();
        LOGGER.info(
            hostUpdateRequired ? "Hosted C6 done; updating slave then host"
                               : "Hosted C6 done; updating slave only"
        );
        return;
    }
    LOGGER.info(
        hasSlaveImage ? "Hosted C6 done; slave skipped; updating host"
                      : "Hosted C6 done; updating host only"
    );
    startHostApply();
}

bool FirmwareOta::startHostedC6Apply() {
#if !defined(BOARD_HOST_P4)
    fail("Hosted C6 update not supported on this host");
    return false;
#else
    if (!hostedIsInitialized()) {
        fail("Hosted C6 not initialized");
        return false;
    }
    ensurePackageFsMutex();
    takePackageFs();
    if (!packageFile) {
        packageFile = LittleFS.open(kPackagePath, "r");
    }
    if (!packageFile
        || !JoinedFirmwareZip::findMembers(
               packageFile, &slaveMember, &hostMember, &hostedC6Member
           )
        || !hostedC6Member.found) {
        givePackageFs();
        fail("Joined package has no hosted-c6.bin");
        return false;
    }
    memberReader.close();
    if (!memberReader.open(packageFile, hostedC6Member)) {
        givePackageFs();
        fail("Failed to open hosted-c6.bin stream");
        return false;
    }
    if (!hostedBeginUpdate()) {
        memberReader.close();
        givePackageFs();
        fail("hostedBeginUpdate failed");
        return false;
    }
    hostedC6ImageSize = hostedC6Member.uncompressedSize;
    imageSize = hostedC6ImageSize;
    imageOffset = 0;
    streamPos = 0;
    currentPhase = Phase::HostedC6;
    givePackageFs();
    LOGGER.info(
        "Hosted C6 SDIO update start size=" + String((unsigned long)hostedC6ImageSize)
    );
    return true;
#endif
}

void FirmwareOta::pumpHostedC6Apply() {
#if !defined(BOARD_HOST_P4)
    fail("Hosted C6 update not supported on this host");
#else
    if (currentPhase != Phase::HostedC6 || !memberReader.isOpen()) {
        return;
    }
    uint8_t buffer[2048];
    takePackageFs();
    const size_t got = memberReader.read(buffer, sizeof(buffer));
    if (got == 0 && memberReader.failed()) {
        givePackageFs();
        fail("Hosted C6 image read failed");
        return;
    }
    if (got > 0) {
        if (!hostedWriteUpdate(buffer, (uint32_t)got)) {
            givePackageFs();
            fail("hostedWriteUpdate failed");
            return;
        }
        imageOffset += (uint32_t)got;
        givePackageFs();
        return;
    }
    givePackageFs();
    memberReader.close();
    if (!hostedEndUpdate()) {
        fail("hostedEndUpdate failed");
        return;
    }
    if (!hostedActivateUpdate()) {
        fail("hostedActivateUpdate failed");
        return;
    }
    delay(500);
    char runningHosted[SPI_STATUS_VERSION_MAX + 1]{};
    if (!readRunningHostedC6Version(runningHosted, sizeof(runningHosted))) {
        fail("Hosted C6 version unreadable after apply");
        return;
    }
    if (expectedHostedC6Version[0] != '\0'
        && strcmp(runningHosted, expectedHostedC6Version) != 0) {
        fail("Hosted C6 version mismatch after apply");
        return;
    }
    hostedC6UpdateRequired = false;
    LOGGER.info("Hosted C6 updated to " + String(runningHosted));
    continueAfterHostedC6();
#endif
}

void FirmwareOta::pumpPackagePrepare() {
    if (!packagePreparePending || currentPhase != Phase::Preparing) {
        return;
    }
    packagePreparePending = false;
    if (!prepareJoinedPackage()) {
        return;
    }
    if (hostedC6UpdateRequired) {
        if (!startHostedC6Apply()) {
            return;
        }
        return;
    }
    if (!slaveUpdateRequired && !hostUpdateRequired) {
        finishNoOpSuccess("host and slave already match package versions");
        return;
    }
    if (!slaveUpdateRequired) {
        LOGGER.info(
            hasSlaveImage ? "Slave update skipped (version match); updating host only"
                          : "Joined package has no slave.bin; updating host only"
        );
        startHostApply();
        return;
    }
    strncpy(preOtaSlaveVersion, INTER_CHIP_HOST.slaveFirmwareVersion(), sizeof(preOtaSlaveVersion) - 1);
    preOtaSlaveVersion[sizeof(preOtaSlaveVersion) - 1] = '\0';
    slaveSpiCycleCount = 0;
    startSlaveUploadCycle();
    LOGGER.info(
        hostUpdateRequired ? "Joined package stream ready, updating slave then host"
                           : "Joined package stream ready, updating slave only"
    );
}

void FirmwareOta::pump() {
    pumpSlaveBegin();
    pumpSlaveApply();
    pumpPackagePrepare();
    if (currentPhase == Phase::HostedC6) {
        pumpHostedC6Apply();
        return;
    }
    if (currentPhase == Phase::Slave) {
        enqueueNextSlaveChunk();
        return;
    }
    if (currentPhase == Phase::VerifyingSlave) {
        pumpSlaveVersionVerify();
        return;
    }
    if (currentPhase == Phase::Host) {
        pumpHostApply();
        return;
    }
    if (currentPhase == Phase::Rebooting && rebootReadyMs != 0 && millis() >= rebootReadyMs) {
        rebootReadyMs = 0;
        hostRestartPending = true;
        currentPhase = Phase::Done;
        LOGGER.info("Host restarting after firmware update");
    }
}

bool FirmwareOta::consumeHostRestart() {
    if (!hostRestartPending) {
        return false;
    }
    hostRestartPending = false;
    return true;
}

bool FirmwareOta::consumeFirmwareRestart() {
    if (!slaveRestartPending) {
        return false;
    }
    slaveRestartPending = false;
    return true;
}

bool FirmwareOta::handleSlaveFrame(const SpiFrame &frame) {
    if (frame.cmd != SpiCmdFirmwareOta || frame.length < 1) {
        return false;
    }
    const uint8_t flags = frame.payload[0];
    if ((flags & SPI_OTA_ABORT) != 0) {
        if (slaveOtaActive || slaveApplyPending || slaveApplyArmed) {
            Update.abort();
        }
        slaveOtaActive = false;
        slaveBeginPending = false;
        slaveApplyPending = false;
        slaveApplyArmed = false;
        slaveApplyWaitTransfers = 0;
        slaveBeginExtraLength = 0;
        slaveBytesRemaining = 0;
        return true;
    }

    bool writeOk = true;
    if ((flags & SPI_OTA_BEGIN) != 0) {
        return queueSlaveBegin(frame);
    }
    // END retry after we already armed/committed: succeed without rewriting.
    if ((flags & SPI_OTA_END) != 0
        && (slaveApplyArmed || slaveApplyPending || slaveRestartPending)) {
        return true;
    }
    if (!slaveOtaActive || slaveBeginPending) {
        return false;
    }
    const uint16_t dataLength = frame.length - 1;
    if (dataLength > 0) {
        uint8_t dataBytes[SPI_MAX_PAYLOAD];
        memcpy(dataBytes, frame.payload + 1, dataLength);
        if (dataLength > slaveBytesRemaining || Update.write(dataBytes, dataLength) != dataLength) {
            writeOk = false;
        } else {
            slaveBytesRemaining -= dataLength;
        }
    }

    if (!writeOk) {
        Update.abort();
        slaveOtaActive = false;
        slaveBytesRemaining = 0;
        return false;
    }

    if ((flags & SPI_OTA_END) == 0) {
        return true;
    }
    if (!slaveOtaActive || slaveBytesRemaining != 0) {
        LOGGER.warning(
            "Slave OTA END rejected remaining=" + String((unsigned long)slaveBytesRemaining)
            + " active=" + String(slaveOtaActive ? 1 : 0)
        );
        Update.abort();
        slaveOtaActive = false;
        slaveBytesRemaining = 0;
        return false;
    }
    // Write is done; ACK must leave SPI before Update.end (flash lock kills replies).
    slaveOtaActive = false;
    // Arm after caller queues the ACK; wait one more completed transfer to clock it out.
    slaveApplyArmed = true;
    slaveApplyPending = false;
    slaveApplyWaitTransfers = 1;
    return true;
}

void FirmwareOta::armSlaveApplyAfterAckDrain() {
    // Kept for call sites that re-arm after reset; END path sets waitTransfers itself.
    if (!INTER_CHIP_SLAVE.hasOutboundPending()) {
        slaveApplyArmed = false;
        slaveApplyPending = true;
        slaveApplyWaitTransfers = 0;
        return;
    }
    slaveApplyArmed = true;
    slaveApplyPending = false;
    slaveApplyWaitTransfers = 1;
}

void FirmwareOta::onSlaveSpiTransferDone() {
    if (!slaveApplyArmed) {
        return;
    }
    if (slaveApplyWaitTransfers > 0) {
        slaveApplyWaitTransfers--;
        return;
    }
    slaveApplyArmed = false;
    slaveApplyPending = true;
}

void FirmwareOta::pumpSlaveApply() {
    if (!slaveApplyPending) {
        return;
    }
    slaveApplyPending = false;
    if (!Update.end(true)) {
        LOGGER.error("Slave firmware Update.end failed");
        Update.abort();
        return;
    }
    slaveRestartPending = true;
    LOGGER.info("Slave firmware image committed");
}

bool FirmwareOta::queueSlaveBegin(const SpiFrame &frame) {
    uint32_t size = 0;
    if (!spiUnpackFirmwareOtaSize(frame.payload, frame.length, &size) || size == 0) {
        return false;
    }
    if (slaveBeginPending) {
        return slaveBeginSeq == frame.seq;
    }
    if (slaveOtaActive) {
        Update.abort();
    }
    slaveOtaActive = true;
    slaveBeginPending = true;
    slaveBeginSeq = frame.seq;
    slaveBytesRemaining = size;
    slaveBeginExtraLength =
        frame.length > SPI_OTA_BEGIN_LEN ? (uint16_t)(frame.length - SPI_OTA_BEGIN_LEN) : 0;
    if (slaveBeginExtraLength > 0) {
        memcpy(slaveBeginExtra, frame.payload + SPI_OTA_BEGIN_LEN, slaveBeginExtraLength);
    }
    LOGGER.info("Slave firmware begin accepted, erase scheduled size=" + String((unsigned long)size));
    return true;
}
