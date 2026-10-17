# Spec Delta

## MODIFIED Requirements

### Requirement: Automation Trigger listen rows are typed

Each Trigger (listening) row under **Triggered by devices** MUST let the operator choose a registered device, a channel when that device has more than one channel (channel control MUST be hidden when only one channel is available), and either a **known parameter** for that device’s classified type or **CUSTOM**. When CUSTOM is selected, the row MUST show cluster and attrId fields for entry (hex). The row MUST also expose **Event type** with options **Change** and **Any value**, default **Change**, placed at the **end** of the row’s fields (after parameter / custom cluster-attr controls and before Remove). Each of those controls MUST show a field label on the **same line** as the control (including a **Field** label for the parameter picker). The Remove control on Trigger rows MUST use the same compact button sizing as Remove controls in Condition and Actions (`btn-inline` / shared editor button style).

#### Scenario: Add listen uses known param

- **WHEN** the operator adds a Trigger listen row for an On/Off device
- **THEN** the row offers known parameters for that type (including Status) and CUSTOM, Event type defaults to Change, and Event type appears after Field

#### Scenario: Labels on same line

- **WHEN** the automation edit dialog shows a Triggered by devices row
- **THEN** Device, Channel (when shown), Field, Event type (and custom cluster/attr when shown) each have a label on the same line as their control

#### Scenario: Custom shows cluster and attr

- **WHEN** the operator selects CUSTOM on a listen row
- **THEN** cluster and attrId inputs appear for that row before Event type

#### Scenario: Remove matches other editors

- **WHEN** the operator views Remove on a Trigger listen row
- **THEN** that control uses the same compact button classes as Condition/Actions Remove controls
