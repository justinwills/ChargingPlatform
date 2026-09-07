# ChargingPlatform

## Folder Layout

- `db/` - shared SQLite database layer and schema.
- `protocol/` - JSON frame encoding/decoding for TCP messages.
- `server/` - listener, client thread, and request dispatcher.
- `client/` - client-side TCP connection wrapper.
- `tests/` - standalone Qt console test projects.

## Source Layout

The root `db/`, `protocol/`, `server/`, and `client/` directories are the active
CMake targets used by the root `CMakeLists.txt`. The client admin page is in
`client/admin/` and is built as part of `ChargingClient`. Use the root targets
for development; the unused mirror directory is not part of the build.

Generated build output belongs in `build/`, and local SQLite runtime/test files
are ignored by Git.

## Test Projects

- `tests/DatabaseTest.pro` runs the database-only test driver.
- `tests/ProtocolTest.pro` starts a local test server/client pair and checks the JSON actions.

Open the `.pro` file you need in Qt Creator, configure the project, then run it.
