[日本語](stuff-folder.ja.md)

# The Stuff Folder

The **Stuff folder** is the folder that holds the settings used by PPM.
It contains the project files, translation files, rename scripts, the initial values of Preferences, and so on.

If you put the Stuff folder in a shared folder and have everyone's PPM use the same Stuff folder, you can manage the project settings in one place (see [Running PPM on a Shared Folder](shared-operation.md)).

## Folder structure

```
stuff/
├─ projects/                 … project files (.ppmprj)
│  └─ sample_project.ppmprj
├─ loc/                      … translation files (.qm)
│  └─ PPM_ja.qm
├─ rename_scripts/           … rename scripts (.js)
│  └─ genga_to_uchikomi.js
├─ LICENSE/                  … license documents
├─ sample_data/              … sample data for the tutorial
└─ defaultusersettings.ini   … initial values of Preferences (optional; created by the administrator)
```

| Folder or file | Contents |
|---|---|
| `projects` | The project files. The file name (without the extension) is the project ID. The **Project** list in the main window shows the projects in this folder (see [Saving and Testing a Project](../designing/saving-and-testing.md)). |
| `loc` | The translation files for the user interface language (see [UI Language and Translation Files](translations.md)). |
| `rename_scripts` | The rename scripts used in the **Copy Folder Contents** step (see [Rename Scripts](../integrations/rename-scripts.md)). |
| `LICENSE` | The license documents of PPM and of the library it uses (Qt). They are shown in **About PPM**. Do not delete them. |
| `sample_data` | The scene files, project info, and logs used by the sample project (see [Tutorial](../getting-started/tutorial.md)). If you do not use the sample project, you can delete this folder together with `sample_project.ppmprj` in the `projects` folder. |
| `defaultusersettings.ini` | The initial values of Preferences (see "Initial values of Preferences" below). It is not included in the distribution; create it if needed. |

When paths such as the root folder are written as relative paths in the project settings, they are relative to the Stuff folder (see [Basics](../designing/basics.md)).

## Specifying the folder in conf.ini

On Windows, PPM reads `conf.ini` in the same folder as `ppm.exe` at startup to find the Stuff folder.

On macOS, place `conf.ini` in `~/Library/Application Support/PPM` (create the directory if needed). This configuration is per user and shared by all PPM builds that user runs. Relative paths are resolved from that folder, and absolute paths must be accessible on the Mac (for example, `/Volumes/anime/PPM/stuff`). See [Building and starting on macOS](../about/contributing.md#building-and-starting-on-macos) for the source-build setup and its limitations.

```ini
[General]
StuffFolderPath=//fileserver/anime/PPM/stuff
```

- A relative path is relative to the directory containing `conf.ini`. On Windows, this is the folder of `ppm.exe`. The `conf.ini` in the distribution has `StuffFolderPath=stuff`, which points to the `stuff` folder next to `ppm.exe`.
- Use slashes (`/`) as path separators. If you use backslashes (`\`), you must write two of them, like `\\` (see [Installation](../getting-started/installation.md)).
- If there is no `conf.ini`, or `StuffFolderPath` is empty, PPM cannot find the Stuff folder and cannot open projects.
- On Windows, there is one `conf.ini` for each `ppm.exe`. If everyone uses one `ppm.exe` in a shared folder, `conf.ini` is also shared by everyone.

After changing the Stuff folder, restart PPM to apply the change.

## Initial values of Preferences

If you put `defaultusersettings.ini` in the Stuff folder, you can set the initial values of everyone's **Preferences** at once.
For example, if CLIP STUDIO PAINT is installed in the same location on every PC in the studio, writing its path here saves each staff member from setting it.

```ini
[UserSettings]
LanguageCode=ja
CspBinaryPath=C:/Program Files/CELSYS/CLIP STUDIO 1.5/CLIP STUDIO PAINT/CLIPStudioPaint.exe
TvpBinaryPath=
OpenToonzBinaryPath=
OpenToonzArgument=
XdtsViewerBinaryPath=
```

| Key | Item in Preferences |
|---|---|
| `LanguageCode` | **Language**. Write the `ja` part of the translation file `PPM_ja.qm`. Leave it empty for English. |
| `TvpBinaryPath` | **TVPaint executable path** |
| `CspBinaryPath` | **Clip Studio executable path** |
| `OpenToonzBinaryPath` | **OpenToonz executable path** |
| `OpenToonzArgument` | **OpenToonz arguments** |
| `XdtsViewerBinaryPath` | **XDTS application path** |

- Keys that are not written, and empty keys, keep the default of Preferences (empty). However, if `LanguageCode` is not written, the operating system's display language is used (English if there is no translation file for that language).
- The **User Name [USER_NAME]** cannot be given an initial value.
- Use slashes (`/`) as path separators. Backslashes are treated the same way as in `conf.ini`.
- The path above is an example. Check the actual installation location.

### Which items use the initial values

The initial values are used for the items that each staff member **has not changed in Preferences**.
Once an item is changed, it is saved in that staff member's settings file (`usersettings.ini`) and takes priority over the initial value from then on (see [Preferences](../using/preferences.md)).

Therefore, if you change `defaultusersettings.ini` later, staff members who have not changed the item get the new value the next time they start PPM.
It is not applied to staff members who have already set the item themselves.

---

[← Back to contents](../index.md)
