[日本語](README.md)

# SETOLIA

Setlist plugin for OBS Studio

## Main Features

- Synchronized timestamp with the streaming time
- Real-time clock display
- Shortcut button to [lyrics search site](https://utaten.com)
- Button to add pre-configured browser source
- Support custom template
- Supports operation from Stream Deck

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

### Changing the server port

The HTTP server uses port `8080` by default. If another application already uses 8080, the dock shows "Port 8080 is not available" in red. Enter another number (1024–65535) under "Server Port" at the bottom of the dock and press "Apply": the server restarts on that port and existing template browser sources are rewritten to the new port. The setting is saved and used on the next start.

If you call `http://127.0.0.1:8080/next` from Stream Deck or similar tools, update the port in those URLs as well.

## License & Acknowledgements

This software is licensed under the GPL-3.0 License.

This project was developed using the [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate) from the [OBS Project](https://github.com/obsproject).

Also, the standard templates included with this software are licensed under the MIT License.
