## MODIFIED Requirements

### Requirement: Scene kind and camera selectors
On the Scenes editor, a fixed dropdown for scene kind (**Plan** | **Camera** | **Custom background**) MUST appear before the **Edit scene** control. The kind selector MUST be approximately **twice** as wide as the prior Plan/Camera-only control so the Custom background label fits. **Edit scene** MUST be visible only when kind is **Plan** (geometry bake editor) and MUST be hidden when kind is **Camera** or **Custom background**. An **Upload image** control MUST be visible only when kind is **Custom background** and MUST be hidden for Plan and Camera. When kind is **Camera**, a fixed dropdown of registered cameras MUST appear after the kind control (where Edit scene would be for Plan). Changing kind MUST switch the authoring model (Plan → bake/pins; Camera → camera + zones; Custom background → uploaded image + pins) without inventing Plan geometry tools on Camera or Custom scenes.

#### Scenario: Kind before Edit scene
- **WHEN** the operator views the scene editor chrome with kind Plan
- **THEN** the kind dropdown is shown before Edit scene

#### Scenario: Edit scene hidden for Camera
- **WHEN** kind is Camera
- **THEN** the Edit scene button is not shown

#### Scenario: Camera picker when Camera
- **WHEN** kind is Camera
- **THEN** a camera dropdown is shown after the kind control and lists registered cameras

#### Scenario: No camera picker for Plan
- **WHEN** kind is Plan
- **THEN** the camera dropdown is not shown

#### Scenario: Custom background option and wider kind select
- **WHEN** the operator opens the scene editor kind dropdown
- **THEN** Custom background is listed and the kind control is about twice the prior width

#### Scenario: Upload image only for Custom background
- **WHEN** kind is Custom background
- **THEN** Upload image is shown and Edit scene is hidden

#### Scenario: Upload image hidden for Plan
- **WHEN** kind is Plan
- **THEN** Upload image is not shown

#### Scenario: Custom background blocked without SD
- **WHEN** the operator selects Custom background and no usable SD card is present
- **THEN** a warning states that an SD card must be installed and the kind selector returns to the previous value

## ADDED Requirements

### Requirement: Full backup includes custom background images
Full backup MUST include each Custom background scene’s stored image bytes from SD in the ZIP (alongside Plan bake JPEGs from LittleFS). Ordinary Export settings MUST continue to omit image bytes and only carry scene metadata (including kind and bakePath/hasBake). Full restore MUST wipe prior custom background files on SD together with Plan bake wipe, then restore custom images from the archive when SD is usable.

#### Scenario: Full backup packs custom image
- **WHEN** the operator runs Full backup and a Custom background scene has an uploaded image on SD
- **THEN** that image is present as a member in the downloaded ZIP

#### Scenario: Full restore restores custom image
- **WHEN** Full restore applies an archive that contains a custom background image member and SD is usable
- **THEN** after apply/reboot that Custom background scene serves the restored image

#### Scenario: Settings export omits custom image bytes
- **WHEN** the operator uses ordinary Export settings
- **THEN** the JSON does not embed custom background image bytes
