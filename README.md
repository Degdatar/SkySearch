# SkySearch

SkySearch searches records loaded by the current Skyrim Anniversary Edition. It provides category and plugin filters, FormID lookup, bounded result lists, and actions for supported records.

The search is live against Skyrim's loaded data.

## Features

- Search loaded NPCs, quests, items, spells, shouts, perks, cells, mods, and other supported record types.
- Filter searches by record category and owning plugin.
- Search by record name or hexadecimal FormID.
- Configure the maximum number of results and whether FormIDs appear in results.
- Perform supported actions: add or spawn eligible items, learn spells/shouts/perks, manage quest state, and teleport to supported actors or cells.
- Provide module settings and documentation for the SkyOS/SkyViewer interface.

## Requirements

- Windows x64 and Visual Studio 2022 with the C++ desktop workload.
- CMake 3.24 or newer.
- Git, for fetching CommonLibSSE-NG.
- vcpkg with `VCPKG_ROOT` set, using the `x64-windows-static-md` triplet.
- Skyrim SE/AE, SKSE, PrismaUI, and the matching SkyOS core at runtime.

## Build

From this directory, configure and build the Release preset:

```powershell
cmake --preset release
cmake --build --preset release --parallel
```

Create an archive of the install tree:

```powershell
cpack --config build/CPackConfig.cmake -G 7Z -C Release
```

The CMake install tree contains the plugin and SkySearch-owned module files under Skyrim's `Data` directory structure. CPack's default generator is ZIP; the command above selects 7-Zip when the CPack 7Z generator is available on the machine.

## SkyOS integration

SkySearch is an optional native app module. `SkyOS.dll` owns the shared PrismaUI shell, search page, event listeners, and settings bridge. It calls these functions exported by `SkySearch.dll`:

- `SkyOS::SearchSkyrim(query, category, modName, maxResults)`
- `SkyOS::ExecuteSkySearchAction(action, formId, count, error, feedback)`

The module's SKSE entry checks for `SkyOS.dll` at data load and remains inactive if the core is missing. Building or installing this repository alone does not provide the shared SkyOS shell; install the matching SkyOS core and PrismaUI as well.

The bundled `data` directory contains only SkySearch resources and registration files: its PrismaUI artwork, SkyViewer module page/settings, app manifest, and SkyDocuments guides. The two source guides are:

- `data/SKSE/Plugins/SkyOS/Mods/SkySearch/SkyDocuments/SkySearch.html` — illustrated player guide.
- `data/SKSE/Plugins/SkyOS/Mods/SkySearch/SkyDocuments/SkySearch Module.md` — concise module reference.

CMake installs both guides to `Data/SKSE/Plugins/SkyOS/Mods/SkySearch/SkyDocuments/`. The shared shell remains in the SkyOS repository.

## Repository layout

```text
src/                         Native search and action plugin
data/PrismaUI/views/          SkySearch logo and banner
data/SKSE/Plugins/SkyOS/Mods/ SkySearch app metadata and guides
                             SkyViewer module page and settings
```

## Configuration

SkySearch settings are managed from SkyViewer → Modules → SkySearch. The shipped defaults are:

| Setting | Default | Range |
| --- | ---: | ---: |
| Open hotkey | Alt + Space | Key binding |
| Close hotkey | Escape | Key binding |
| Default inventory item count | 1 | 1–2,140,000,000 |
| Default world spawn count | 1 | 1–100 |
| Maximum results per search | 500 | 1–5,000 |
| Show FormIDs | On | On or off |

Per-user settings are stored by SkyOS under `%LOCALAPPDATA%/SkyOS/SkyViewer/SkySearch/settings.json`.
