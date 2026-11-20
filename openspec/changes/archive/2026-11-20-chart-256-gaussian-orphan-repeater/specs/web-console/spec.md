## MODIFIED Requirements

### Requirement: History chart smooth defaults without EMA
On both the operator and simple-user history chart UIs, numeric smooth MUST use **Gaussian-weighted** window smoothing only (no flat average as the offered algorithm). EMA MUST NOT be offered or applied. When the history chart opens, Smooth MUST be unchecked by default and the samples field MUST default to **3**. The same defaults and EMA removal MUST ship on both pages.

Let `U` be the operator-entered samples value (`U ≥ 1`). The real window length MUST be `N = 2U + 1`. Weights MUST be a Gaussian with the center sample at the middle of the window (`x = 0`) and the first and last samples at `x = ±2/3` (σ = 1: `w = exp(-0.5 · x²)`). Each charted point MUST be the weight-normalized sum of available neighbors in that window (edge truncation renormalizes).

#### Scenario: Defaults on open
- **WHEN** a user opens history for a numeric binding on either page
- **THEN** Smooth is unchecked and the samples field shows 3

#### Scenario: No EMA control
- **WHEN** Smooth is enabled on the history chart
- **THEN** there is no Ema/EMA type option and smoothing uses the Gaussian window described above

#### Scenario: Window length from user samples
- **WHEN** Smooth is enabled and the samples field is 2
- **THEN** the smoother uses five underlying samples (`2·2+1`) with the center weight on the middle sample

## ADDED Requirements

### Requirement: History chart requests up to 256 points
On both the operator and simple-user history chart UIs, each history fetch MUST request `maxPoints=256` (or omit the param only when the server default is already 256). The chart MUST render whatever point count the API returns up to that bound.

#### Scenario: Chart fetch asks for 256
- **WHEN** either page loads history for a binding
- **THEN** the request asks for at most 256 points

### Requirement: Orphan repeater device ROUTER is editable
When the operator edits a registered device whose `routerId` is non-zero and that id is **not** among currently paired interconnect Repeaters (orphan / absent repeater), the device parameter dialog ROUTER control MUST be an editable fixed `<select>` listing **local** (`routerId` 0) and every currently paired Repeater (by name / id). When the stored id is not among those options, the select MUST default to **local**. When the device’s repeater is still paired or `routerId` is already local, ROUTER MUST remain read-only as today. Saving with the editable select MUST POST the chosen `routerId` so the device can be reassigned without deleting it.

#### Scenario: Orphan shows select
- **WHEN** the operator opens Edit for a device with `routerId` 5 and no paired peer has router id 5
- **THEN** ROUTER is a dropdown that includes local and each paired Repeater

#### Scenario: Still-paired stays read-only
- **WHEN** the operator opens Edit for a device whose `routerId` matches a paired Repeater
- **THEN** ROUTER stays read-only text (not a free-text edit of arbitrary ids)

#### Scenario: Save reassigns without delete
- **WHEN** the operator chooses a paired Repeater (or local) in the orphan ROUTER select and Saves
- **THEN** the device remains registered and its stored `routerId` becomes the chosen value
