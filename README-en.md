[日本語](README.md)

# SETOLIA

Setlist plugin for OBS Studio

## Main Features

- Synchronized timestamp with the streaming time
- Real-time clock display
- Shortcut button to [lyrics search site](https://utaten.com)
- Button to add pre-configured browser source
- Support custom template
- Supports operation from Stream Deck and hotkeys

## What's new in 1.3.0

Existing settings and scenes keep working after the update; no action is required.

- Supports OBS 33 (both the new plugin layout and the layout of OBS 32 and earlier).
- Templates are served by the plugin's HTTP server. Template browser sources added with earlier versions are converted automatically.
- The HTTP server port can be changed in the dock. If the new port is in use, the server keeps running on the previous port.
- Custom templates can be placed in the user template folder inside the OBS configuration folder. Custom templates that earlier versions kept in the bundled template folder are copied there automatically and survive uninstalling.
- New option to reject next-song requests (`/next`) from web pages (off by default).
- New "SETOLIA: Next Song" hotkey.
- Settings are saved automatically after each edit, so recent edits survive an OBS crash.
- Timestamps no longer drift during long streams, and they also run when OBS is already streaming at startup.
- When you restart OBS and resume the same broadcast, the timestamp can continue from the previous stream ("Continue from the previous stream" button).
- Name and version are shown in the plugin manager of OBS 32 and later.

## Installation

Binaries for Windows, MacOS, and Linux are available in the [Releases](https://github.com/Yagami-Huki/setolia/releases) section.

Also, the latest installer for Windows is available at [Booth](https://logicbox.booth.pm/items/7541386).

The Windows build supports both the new plugin layout introduced in OBS 33 and the layout used by OBS 32 and earlier.

### Windows (manual install from the zip)

Extract the zip into `C:\ProgramData\obs-studio\plugins\setolia\`. The result should look like this:

```
C:\ProgramData\obs-studio\plugins\setolia\
├── setolia.dll          (loaded by OBS 33 and later)
├── bin\64bit\setolia.dll (loaded by OBS 32 and earlier)
└── data\
```

Both `setolia.dll` files are identical. OBS 33 and later load the one at the top level and skip the copy in `bin\64bit`.

### Windows (portable OBS)

- OBS 33 and later: `<OBS>\plugins\setolia\setolia.dll` and `<OBS>\plugins\setolia\data\` (portable OBS ignores plugins in `C:\ProgramData`)
- OBS 32 and earlier: `<OBS>\obs-plugins\64bit\setolia.dll` and `<OBS>\data\obs-plugins\setolia\`

### Upgrading from older versions

If an older version is still in `C:\Program Files\obs-studio\obs-plugins\64bit\setolia.dll`, OBS 32 and earlier may load the plugin twice. The installer detects this and asks whether to remove the old files. If you install manually from the zip, delete the old files (`obs-plugins\64bit\setolia.dll` and `data\obs-plugins\setolia\`) yourself.

## Usage

The English version of the user manual is currently under preparation.

Please refer to the Japanese manual available at [Booth](https://logicbox.booth.pm/items/7541386).

### OBS 33 and later

OBS 33 blocks requests to `127.0.0.1` from browser sources loaded as local files, so templates are now served by the plugin's HTTP server (`http://127.0.0.1:8080/templates/...`).

Template browser sources added with earlier versions are converted to the URL form automatically when OBS starts and when the scene collection changes, so you don't need to re-add your templates. Sources that still point at an old install location (`C:\Program Files\obs-studio\data\obs-plugins\setolia\templates\...`, or `<OBS>\data\obs-plugins\setolia\templates\...` of a portable OBS 32) are converted too, as long as a template with the same name exists.

### Timestamps after resuming an interrupted stream

When OBS is closed (or crashes) during a stream, YouTube keeps the broadcast waiting for a while. If you resume within that time, the archive continues from before the interruption, but the timestamp would start again from 0.

If a stream starts less than a minute after the previous one stopped, a "Continue from the previous stream (started HH:MM)" button appears below the timestamp. Pressing it makes the timestamp count from the start of the previous stream, including the interruption (matching the archive).

- If you don't press it, the timestamp starts from 0 as before. Don't press it when you start a different broadcast.
- Songs moved on before pressing keep their timestamps; press it right after resuming, before the first song change.
- Pressing it after each interruption keeps counting from the very first start.
- While streaming, the start time is recorded in `stream-session.json` (SETOLIA config folder) every 10 seconds, so this also works after OBS crashes.

### Next song with a hotkey

"SETOLIA: Next Song" is listed in OBS under Settings → Hotkeys. The assigned key does the same as the dock's "Next Song" button and `POST /next`. The binding is stored in the SETOLIA settings and restored on the next start.

Stream Deck can trigger it too, by sending the assigned key with its standard "Hotkey" action. This does not use HTTP (`/next`), so it is also an alternative if you want to enable "Reject next-song requests from browsers (web pages)" below.

### Rejecting next-song requests from web pages (optional)

`POST /next` (next song) has no authentication, so web pages opened in a browser can call it too. When "Reject next-song requests from browsers (web pages)" is enabled in the dock, `/next` is accepted only in the following cases and answered with `403` otherwise. It is disabled by default, which keeps the previous behavior.

- No `Origin` header (curl, most Stream Deck plugins)
- `Origin` is `http://127.0.0.1:<port>` or `http://localhost:<port>`

Some Stream Deck plugins send requests from an embedded browser with an `Origin` header; with the option enabled they can no longer switch songs (use the hotkey above instead). Whenever `/next` receives an `Origin` header, the OBS log shows `POST /next with Origin '...'` regardless of the setting (once per origin and session). Trigger a song change from your plugin and check the log before enabling the option.

### Changing the server port

The HTTP server uses port `8080` by default. If another application already uses 8080, the dock shows "Port 8080 is not available" in red. Enter another number (1024–65535) under "Server Port" at the bottom of the dock and press "Apply": the server restarts on that port and existing template browser sources are rewritten to the new port. The setting is saved and used on the next start.

If you call `http://127.0.0.1:8080/next` from Stream Deck or similar tools, update the port in those URLs as well.

If the new port is used by another application, the server keeps running on the previous port and the dock shows "Port <new> is not available (still running on <old>)". The setting is not changed in that case.

### Custom templates

Custom templates go into the user template folder inside the OBS configuration folder. "Open Templates Folder" in the dock opens it.

| OS | User template folder |
| --- | --- |
| Windows | `%APPDATA%\obs-studio\plugin_config\setolia\templates` |
| macOS | `~/Library/Application Support/obs-studio/plugin_config/setolia/templates` |
| Linux | `~/.config/obs-studio/plugin_config/setolia/templates` |

- Copy a bundled template folder (`data\templates` of the install folder) into this folder, rename it and edit it. While the folder is empty, a `README.txt` with these instructions is placed in it.
- The template list shows both the user templates and the bundled templates. A folder with the same name as a bundled template takes precedence (per file: files missing in the user folder are taken from the bundled template).
- The user template folder is not removed when SETOLIA is uninstalled or updated.
- Custom templates that earlier versions expected in the bundled template folder (e.g. `C:\ProgramData\obs-studio\plugins\setolia\data\templates`) are copied to the user template folder automatically on startup (the originals are left in place). Template URLs do not change, so browser sources keep working without being re-added.

## License & Acknowledgements

This software is licensed under the GPL-3.0 License.

This project was developed using the [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate) from the [OBS Project](https://github.com/obsproject).

Also, the standard templates included with this software are licensed under the MIT License.
