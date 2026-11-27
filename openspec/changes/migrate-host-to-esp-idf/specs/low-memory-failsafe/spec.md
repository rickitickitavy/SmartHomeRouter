## MODIFIED Requirements

### Requirement: Fail-safe stops work then frees AsyncWebServer
After the trip latches, the host SHALL stop ongoing application serving/work as needed to avoid further heap growth, then SHALL stop, destroy, and free the HTTP console server so its listen sockets, handlers’ connection state, and related buffers are released. After this step the HTTP console NEED NOT remain reachable until reboot completes.

#### Scenario: Web server torn down
- **WHEN** the low-memory fail-safe runs
- **THEN** the HTTP console server is stopped/destroyed and its memory is freed before the history save step completes

### Requirement: Fail-safe saves history then reboots
After HTTP console server teardown, the host SHALL save historical data using the same snapshot algorithm used for a normal pre-restart history flush (current backend: LittleFS or SD as already selected by product logic). The host SHALL then reboot. Failure to save history MUST NOT skip the reboot indefinitely; the host MAY log and still reboot.

#### Scenario: History then reboot
- **WHEN** the fail-safe has torn down the web server and history samples exist
- **THEN** the host attempts the current history snapshot persist and then reboots

#### Scenario: Persist failure still reboots
- **WHEN** the history snapshot persist fails or is skipped during the fail-safe
- **THEN** the host still reboots
