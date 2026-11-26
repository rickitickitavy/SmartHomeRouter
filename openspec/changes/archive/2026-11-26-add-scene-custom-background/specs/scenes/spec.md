## MODIFIED Requirements

### Requirement: Scene kind Plan or Camera
Each scene MUST have kind `plan`, `camera`, or `custom`. Missing kind on stored scenes MUST behave as `plan`. Camera scenes MUST use a registered camera as background and active zones. Plan scenes MUST keep bake-image and pin authoring. Custom background scenes (`custom`) MUST author pins like Plan and MUST use an operator-uploaded still image as the background instead of a geometry-generated bake. Camera scenes MUST NOT use icon/gauge pins in the operator or simple-user UI. Custom background scenes MUST NOT use Camera zones.

#### Scenario: Legacy scene is Plan
- **WHEN** a stored scene has no kind field
- **THEN** it behaves as Plan (bake + pins)

#### Scenario: Camera scene has no pins UI
- **WHEN** the operator opens a Camera scene for live view or bind editing
- **THEN** pin placement/controls are not offered; active zones are used instead

#### Scenario: Custom background uses pins like Plan
- **WHEN** the operator opens a Custom background scene for bind editing or live view
- **THEN** icon/gauge pin authoring and display work like Plan, and Camera zones are not offered

## ADDED Requirements

### Requirement: Custom background image storage and limits
A Custom background scene’s background image MUST be stored on the SD card (not LittleFS). Upload MUST accept PNG and JPEG still images (and equivalent lightweight formats the host serves as files) with a maximum body size of **1 MiB**. Upload MUST fail with a clear error when SD is missing or unusable, when the body exceeds 1 MiB, or when the format is not accepted. Successful upload MUST update the scene’s bake/image reference so Place and simple-user can load it.

#### Scenario: Upload stores on SD
- **WHEN** the operator uploads a valid ≤1 MiB JPEG for a Custom background scene and SD is usable
- **THEN** the image is stored on the SD card and subsequent bake/image GET returns that image

#### Scenario: Upload rejected without SD
- **WHEN** the operator uploads an image for a Custom background scene and SD is missing or unusable
- **THEN** the host rejects the upload with a clear error and does not write the image to LittleFS

#### Scenario: Oversized upload rejected
- **WHEN** the operator uploads an image larger than 1 MiB for a Custom background scene
- **THEN** the host rejects the upload

### Requirement: Custom background has no geometry editor
Custom background scenes MUST NOT open the Plan geometry **Edit scene** flow. Background replacement MUST be via upload only. Changing kind away from `custom` MUST follow existing kind-switch rules without inventing Plan geometry tools on Custom scenes.

#### Scenario: Edit scene hidden for Custom background
- **WHEN** scene kind is Custom background
- **THEN** the Edit scene (geometry) control is not shown
