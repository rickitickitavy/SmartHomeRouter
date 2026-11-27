#if defined(BOARD_ROLE_SLAVE)

#include "InterChipHost.h"

// Slave IDF image does not orchestrate host SPI OTA; FirmwareOta still references
// INTER_CHIP_HOST from shared translation units. Provide empty host stubs for link.

InterChipHost INTER_CHIP_HOST;

void InterChipHost::begin() {}
void InterChipHost::resetSlaveSynchronous() {}
void InterChipHost::pump() {}
bool InterChipHost::tryEnqueue(uint8_t, const uint8_t *, uint16_t) {
    return false;
}
bool InterChipHost::tryEnqueueWithSeq(uint8_t, uint8_t, const uint8_t *, uint16_t) {
    return false;
}
uint8_t InterChipHost::lastEnqueuedSeq() const {
    return 0;
}
bool InterChipHost::isNormal() const {
    return false;
}
bool InterChipHost::isLinkHealthy() const {
    return false;
}
HostBringupState InterChipHost::bringupState() const {
    return HostBringupReset;
}
void InterChipHost::setSettingsSource(uint8_t, uint8_t) {}
void InterChipHost::setClockHz(uint32_t) {}
uint32_t InterChipHost::clockHz() const {
    return 0;
}
void InterChipHost::pushLedBrightness(uint8_t, uint8_t) {}
void InterChipHost::setEventHandler(EventFn) {}
void InterChipHost::requestTimeSync() {}
void InterChipHost::noteKeepaliveQuiet() {}
void InterChipHost::holdForFirmwareOta() {}
bool InterChipHost::isOutboundIdle() const {
    return true;
}
const char *InterChipHost::slaveFirmwareVersion() const {
    return "";
}
void InterChipHost::clearSlaveFirmwareVersion() {}
bool InterChipHost::isSlaveResetPaused() const {
    return false;
}
uint32_t InterChipHost::slaveResetPauseRemainingMs() const {
    return 0;
}
void InterChipHost::startSlaveResetPause() {}
void InterChipHost::cancelSlaveResetPause() {}

#endif
