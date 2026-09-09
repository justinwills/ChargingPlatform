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

## Tencent Map Navigation

The station detail page opens an in-app navigation page with driving/walking
route selection. Install the Qt `WebEngineWidgets` component in the selected
Qt kit to display the Tencent route page inside the client. If the component is
not available, the project still builds and shows a link that opens the same
route in the system browser. In the current local Qt 6.11.2 installation,
`WebEngineWidgets` is available in the `msvc2022_64` kit, not the MinGW kit.

Before starting the server, set `TENCENT_MAP_KEY` to a Tencent Location Service
WebService key. The client uses `TENCENT_MAP_REFERER` for the Tencent URI API;
when it is omitted, it falls back to `TENCENT_MAP_KEY`.

```powershell
$env:TENCENT_MAP_KEY = "your-webservice-key"
$env:TENCENT_MAP_REFERER = "your-browser-key"
```
