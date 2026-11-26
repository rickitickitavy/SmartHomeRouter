## ADDED Requirements

### Requirement: Custom scene backgrounds on SD
On P4 with usable MicroSD, Custom background images MUST live in a dedicated SD directory (not LittleFS). The host MUST mount for upload/serve/backup/restore I/O, then unmount when the session ends. Hosts without usable SD MUST NOT silently store those images on flash.

#### Scenario: Upload writes under SD custom directory
- **WHEN** a Custom background image is uploaded successfully on P4 with SD ready
- **THEN** the file exists under the SD custom-background directory and not under LittleFS scene-bakes

#### Scenario: Unmount after custom image session
- **WHEN** a custom-background upload or serve mount session finishes
- **THEN** the SD filesystem is unmounted afterward (unless another in-progress SD session still needs it)

#### Scenario: No flash fallback without SD
- **WHEN** SD is missing or unusable and a custom background image write is requested
- **THEN** the host does not write that image to LittleFS as a substitute
