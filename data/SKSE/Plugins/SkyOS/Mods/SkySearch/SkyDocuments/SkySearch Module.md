# SkySearch

## Features

- Searches records loaded in the current Skyrim session, including items, actors, locations, quests, spells, and other supported record categories.
- Filters results by category and plugin, displays FormIDs when enabled, and caps large result sets using the SkyViewer `maxResults` setting.
- Numeric limits are shown below the fields: `maxResults` 1–5000, inventory item count 1–2140000000, and world spawn count 1–100.
- Provides supported actions for adding or spawning items, learning supported spell records, teleporting to supported targets, and operating on supported quests.
- Keeps no copied record database. Each request searches the records currently loaded by Skyrim.

## Changelog

- **Standalone app module:** native search and record-action code ships in `SkySearch.dll`; its SkyViewer module, logo, page, and guide ship in the SkySearch package.
- **Existing behavior retained:** category and plugin filters, result limits, FormID display, and supported in-game actions remain available through the SkyOS home page.

## Requirements

- Skyrim Special Edition / Anniversary Edition Latest Version (supported runtime).
- SKSE Latest Version.
- PrismaUI Latest Version.
- SkyOS Latest Version.

## Files and saved data

- Module page and settings schema: `Data/SKSE/Plugins/SkyOS/Mods/SkyViewer/SkySearch/`.
- SkyDocuments guide: `Data/SKSE/Plugins/SkyOS/Mods/SkySearch/SkyDocuments/`.
- Per-user module settings: `%LOCALAPPDATA%/SkyOS/SkyViewer/SkySearch/settings.json`.
- SkySearch does not maintain a separate search index or persistent game-data folder.

