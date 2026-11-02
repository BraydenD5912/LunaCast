# LumaCast

**A revamped live streaming and recording app, built as a fork of [OBS Studio](https://obsproject.com/).**

LumaCast keeps everything that makes OBS a powerful broadcaster — scene composition, audio mixing, encoding, recording, replay buffer, and the plugin ecosystem — and adds a fresh identity plus a set of everyday quality-of-life improvements.

> ### Credit where it's due
> LumaCast is a fork of **OBS Studio**, created by the [OBS Project](https://obsproject.com/) and its many contributors. The vast majority of this codebase is their work and LumaCast would not exist without it.
>
> This project is **not affiliated with, sponsored by, or endorsed by the OBS Project**. "OBS" and "OBS Studio" are the property of their respective owners. See [Built on OBS Studio](#built-on-obs-studio) and [License](#license) below.

---

## What's new in LumaCast

### A new identity

- Renamed application, executable (`LumaCast.exe`), window titles, dialogs, and about screen.
- Custom LumaCast icon set, splash screen, and branding assets.
- A bundled **LumaCast theme** — deep space navy surfaces with violet highlights and cyan accents (based on the Yami theme).
- Separate configuration directory at `%APPDATA%\LumaCast`, so LumaCast profiles and scene collections do not collide with an existing OBS install.

### Pause Stream (privacy pause)

A one-click way to hide your screen without stopping the stream:

- Switches the program output to a branded LumaCast privacy screen.
- Mutes audio sources while paused and restores the previous scene **and mute states** on resume.
- Toggle from the main controls or with the **Pause Stream / Resume Stream** hotkeys (Settings → Hotkeys).
- Shows a status message in the main window so you always know the stream is paused.

### Collapsible scene folders

The Scenes dock now supports real folders:

- **New Scene Folder** from the Scenes context menu or toolbar.
- Drag scenes onto a folder to assign them.
- Expand and collapse folders to keep large scene collections manageable.
- Rename folders, or **Remove Folder (Keep Scenes)** to ungroup without deleting anything.
- Folder state and membership persist inside the scene collection.

### Tabbed Sources dock

The Sources dock can be split into tabs:

- **All Sources** plus any number of custom tabs.
- Create a tab from the **+** button or the Sources context menu.
- Drag a source onto a tab (or use **Move to Tab**) to assign it.
- Right-click a tab to rename or remove it.
- New sources are automatically placed in the active tab.
- Renaming a source or a tab keeps memberships intact; removing a source or tab cleans them up.
- Tabs and memberships are saved with the scene collection.

### Unified Asset Manager

**Tools → Asset Manager...** scans the current scene collection and reports every file and font it depends on:

- Covers image sources, media sources, browser sources with a local file, slideshow files, VLC playlists, and fonts used by text sources.
- Status column shows **OK**, **Missing**, **Remote**, or **Not installed**.
- **Replace...** points a source at a new file, including individual entries inside slideshow and VLC playlist arrays.
- **Change Font...** swaps a text source's font family while keeping its size, style, and effects.
- **Show in Folder** reveals the selected asset in Explorer.
- Summary line counts total assets and missing files at a glance; **Refresh** rescans after changes outside the app.

### Expanded audio bitrate presets

Simple and Advanced output audio bitrate dropdowns now include additional low and mid-range options — **32, 48, 80, and 112 kbps** — alongside the existing 64–320 kbps presets, for low-bandwidth streams and multi-track recordings.

---

## Built on OBS Studio

LumaCast is a modified distribution of OBS Studio. Huge thanks to the OBS Project and every contributor to the upstream project:

- Website: <https://obsproject.com/>
- Source code: <https://github.com/obsproject/obs-studio>
- Documentation and help: <https://obsproject.com/kb>
- Community: <https://obsproject.com/community>

The original OBS Studio `AUTHORS` and license files are retained in this repository. Upstream OBS documentation applies to all core functionality that LumaCast does not modify.

---

## Building from source

### Requirements

- Windows 10/11 (x64) — the tested target for this fork.
- Visual Studio 2022 or newer with the **Desktop development with C++** workload (the included `windows-x64` preset targets the Visual Studio 18 2026 generator).
- CMake 3.28 or newer.
- Git and the OBS Studio Windows dependency bundle (see the upstream [build instructions](https://github.com/obsproject/obs-studio/wiki/Build-Instructions-For-Windows) if you need to set up dependencies from scratch).

### Configure, build, and install

From the repository root:

```bat
cmake --preset windows-x64 -DENABLE_BROWSER=OFF
cmake --build build_x64 --config RelWithDebInfo --parallel 8
cmake --install build_x64 --config RelWithDebInfo
```

This fork's Windows build was configured with:

- `ENABLE_BROWSER=OFF`
- `ENABLE_SCRIPTING=ON`
- `ENABLE_VLC=ON`
- `ENABLE_WEBSOCKET=ON`

### Run

The build produces a self-contained run directory. Launch the executable **from its own folder** so Qt and the data files resolve correctly:

```bat
cd build_x64\rundir\RelWithDebInfo\bin\64bit
LumaCast.exe
```

`cmake --install` places the app under `C:\Program Files\obs-studio` by default, with LumaCast data in `data\lumacast`.

> Windows x64 is the configuration tested by this fork. OBS's macOS and Linux build instructions should still apply, but those platforms have not been verified here.

---

## Known limitations

- **Browser sources are disabled** in this build (`ENABLE_BROWSER=OFF`).
- Only the **English (en-US)** locale has been rebranded; other translations still use OBS strings.
- The organization domain (`lumacast.example`) and service URLs are placeholders; the OBS update, crash-reporting, and analytics endpoints are intentionally not used.
- This is an early fork — features are implemented against the current OBS codebase and may need adjustment when rebasing on newer upstream releases.

---

## License

LumaCast is free software distributed under the **GNU General Public License v2.0 or later**, the same license as OBS Studio.

- Full license text: [`COPYING`](COPYING)
- Upstream project: <https://github.com/obsproject/obs-studio>

OBS Studio is Copyright (C) the OBS Project contributors. LumaCast's modifications are provided under the same GPL-2.0-or-later terms. If you redistribute LumaCast, you must also comply with the GPL, including providing the corresponding source code.
