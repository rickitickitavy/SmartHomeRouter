# Spec Delta

## MODIFIED Requirements

### Requirement: Remember me is ignored for admin
A successful login MAY request Remember me. When the user has `isAdmin`, the host MUST ignore Remember me. An `isAdmin` session MUST expire after **10 minutes with no operator activity**; operator activity MUST reset that idle timer. Automatic authenticated requests (session-alive polls, status polls, log polls, and other requests the console issues without a pointer or keyboard action) MUST NOT reset the idle timer. The session cookie MUST remain valid in the browser for at least the remaining idle window so cookie `Max-Age` from login MUST NOT log the operator out while idle time has not elapsed. When the user is a simple user and Remember me is set, the host MUST issue a longer-lived session that survives a browser restart and a host reboot until that remember lifetime ends or the user is blocked, deleted, or has their password changed. When the user is not `isAdmin`, not a simple user, and Remember me is set, the host MUST keep today’s longer-lived session that survives a browser restart (host reboot survival for those operator roles is unchanged by this requirement).

#### Scenario: Admin checks Remember me
- **WHEN** `admin` logs in with Remember me checked
- **THEN** the session still expires after 10 minutes idle, the same as if Remember me were unchecked

#### Scenario: Admin idle timeout
- **WHEN** an `isAdmin` session has had no operator activity for 10 minutes
- **THEN** the next protected API call is rejected and the operator must sign in again

#### Scenario: Admin activity resets idle
- **WHEN** an `isAdmin` operator uses the signed-in console at 9 minutes idle
- **THEN** the idle timer restarts and the session stays valid

#### Scenario: Admin poll does not reset idle
- **WHEN** an `isAdmin` session sits unused for 10 minutes while the console still polls session status
- **THEN** the session expires and the operator must sign in again

#### Scenario: Admin cookie does not expire on login wall-clock
- **WHEN** an `isAdmin` operator uses the console continuously for more than 10 minutes after login
- **THEN** the browser still sends the session and the host still accepts it

#### Scenario: Simple user Remember me
- **WHEN** a simple user logs in with Remember me checked
- **THEN** a later browser restart still presents a valid session until it expires

#### Scenario: Simple user Remember me survives host reboot
- **WHEN** a simple user logs in with Remember me checked, the host reboots, and the same browser returns with that cookie before the remember lifetime ends
- **THEN** protected APIs accept the session and the simple-user control page opens without asking for the password again

#### Scenario: Simple user without Remember me loses session on reboot
- **WHEN** a simple user logs in with Remember me unchecked and the host reboots
- **THEN** the next console visit shows the login form until that user signs in again

#### Scenario: Password change ends remember session
- **WHEN** a simple user had Remember me and an operator changes that user’s password
- **THEN** the old remember cookie is rejected after the change

## ADDED Requirements

### Requirement: Remember-me proof is not stored in ROM
The host MUST NOT persist remember-me or session slot state to LittleFS, NVS, EEPROM, SPIFFS, or any other ROM or flash store. Durable remember-me proof MUST be carried by the client cookie (or equivalent client-held credential) and revalidated against the in-memory users table after boot. The existing users-table file on LittleFS remains the store for user accounts only, not for session or remember-me records.

#### Scenario: No session file after remember login
- **WHEN** a simple user logs in with Remember me checked
- **THEN** the host does not write a session or remember-me record to LittleFS or any other ROM

#### Scenario: Reboot still validates cookie without ROM session table
- **WHEN** the host reboots with no ROM session store and a valid simple-user Remember me cookie is presented
- **THEN** the host accepts the session using the loaded users table and the cookie proof
