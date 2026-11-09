[日本語](README.md)

# SETOLIA

Setlist plugin for OBS Studio

## Main Features

- Synchronized timestamp with the streaming time
- Real-time clock display
- Shortcut button to [lyrics search site](<(https://utaten.com)>)
- Button to add pre-configured browser source
- Support custom template
- Supports operation from Stream Deck

## Installation

Binaries for Windows, MacOS, and Linux are available in the [Releases](https://github.com/Yagami-Huki/setolia/releases) section.

Also, the latest installer for Windows is available at [Booth](https://logicbox.booth.pm/items/7541386).

## Usage

The English version of the user manual is currently under preparation.

Please refer to the Japanese manual available at [Booth](https://logicbox.booth.pm/items/7541386).

### OBS 33 and later

OBS 33 blocks requests to `127.0.0.1` from browser sources loaded as local files, so templates are now served by the plugin's HTTP server (`http://127.0.0.1:8080/templates/...`).

Template browser sources added with earlier versions are converted to the URL form automatically when OBS starts and when the scene collection changes, so you don't need to re-add your templates.

## License & Acknowledgements

This software is licensed under the GPL-3.0 License.

This project was developed using the [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate) from the [OBS Project](https://github.com/obsproject).

Also, the standard templates included with this software are licensed under the MIT License.
