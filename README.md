# ChargingPlatform

## Folder Layout

- `db/` - shared SQLite database layer and schema.
- `protocol/` - JSON frame encoding/decoding for TCP messages.
- `server/` - backend listener, client thread, and request dispatcher.
- `client/` - client-side TCP connection wrapper.
- `tests/` - standalone Qt console test projects.

## Test Projects

- `tests/DatabaseTest.pro` runs the database-only test driver.
- `tests/ProtocolTest.pro` starts a local test server/client pair and checks the JSON actions.

Open the `.pro` file you need in Qt Creator, configure the project, then run it.
