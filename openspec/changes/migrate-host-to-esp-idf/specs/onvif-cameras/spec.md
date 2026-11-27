## MODIFIED Requirements

### Requirement: Discover uses WS-Discovery then HTTP sweep
Discover MUST run ONVIF WS-Discovery on the host service loop only (no discovery UDP from HTTP console server worker tasks). After the multicast window, Discover MUST sweep the local /24 for HTTP `POST /onvif/device_service` on ports 80 and 8080 with short connect timeouts. A host MUST be added only when the HTTP response body shows ONVIF/SOAP markers (not a generic HTTP 200 or HTML login page). Discover MUST NOT block the host service loop with multi-hundred-millisecond drain delay loops.

#### Scenario: HTTP sweep requires ONVIF markers
- **WHEN** Discover probes a host whose HTTP response is HTML without ONVIF/SOAP markers
- **THEN** that host is not added to the discovered list

#### Scenario: Discover does not use WiFiUDP from web tasks
- **WHEN** the operator starts Discover from the Cameras UI
- **THEN** WS-Discovery UDP runs on the host service loop task only
