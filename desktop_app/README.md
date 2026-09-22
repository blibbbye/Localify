# Localify Desktop

This desktop app wraps the current Localify website from the repository's `main` branch.

## What is bundled

- Current `index.html`
- Current `covers.json`
- Current `covers/` artwork
- Current `favicon.png`
- Current `Localify Discord Player.exe`
- Electron desktop shell
- Windows NSIS installer
- Portable Windows EXE

The renderer is the Localify website itself rather than a separate recreation, so UI/features from the current site stay together.

## Saving

Electron's persistent user-data root is:

`%APPDATA%\\Localify Desktop\\files`

The app creates `audio`, `covers`, and `presets` folders there and exposes an "Open Localify Files" menu item.

## Building

Run `build-installer.bat`.

The generated files are placed in `release\\`.

## Discord

The repository's current native Discord player EXE is bundled with the desktop build and can be started from File → Launch Localify Discord Player.

## Note

The wrapper intentionally does not copy the older HTML snapshot or older connector code: it packages the current repository website directly.
