[日本語](preferences.ja.md)

# Preferences

**Preferences** are settings for each PC user.
On Windows, choose **Preferences** from the gear menu. On macOS, open settings from the **PPM** application menu or press **Command+,**.

The procedure for setting them up when you first use PPM is explained in [Initial Setup](../getting-started/initial-setup.md).
This page summarizes what each item does and where the settings are saved.

![The Preferences dialog](../images/ja/getting-started/preferences.png)
<!-- English screenshot to be made: images/en/getting-started/preferences.png -->

## Items

| Item | Contents |
|---|---|
| **Language** | The language of the PPM user interface. Choose from the languages of the translation files in the `loc` folder of the Stuff folder. "English" uses no translation. The change is applied the next time you start PPM. |
| **TVPaint executable path** | The application for opening `.tvpp` files. |
| **Clip Studio executable path** | The application for opening `.clip` files. |
| **XDTS application path** | The application for opening timesheets (`.xdts`), such as XDTS Viewer or Toei Animation Digital Exposure Sheet. |
| **OpenToonz executable path** | The application for opening `.tnz` files. |
| **OpenToonz arguments** | Startup options added when opening `.tnz` files in OpenToonz. They are passed before the path of the file. You can write several options separated by spaces. Enclose a value that contains spaces in `"`. |
| **User Name [USER_NAME]** | The name substituted for `[USER_NAME]` in the project settings. If empty, the login name is used. |

On macOS, use the **...** button to select an application (`.app`), or enter the path to its executable. PPM asks macOS to open TVPaint and CLIP STUDIO PAINT drawings in the selected application. For a configured XDTS viewer, PPM launches the viewer with the timesheet path as a command-line argument. For an OpenToonz `.app`, PPM starts a new instance and passes the scene path after any startup options.

For an item whose application path is empty, the operating system opens the file with its default application for that extension.
In that case, the OpenToonz arguments are not used.

If the file at an application path cannot be found, or the translation file of the selected language cannot be found, a warning is shown when PPM starts.

## Shared folder mappings

Open **Shared folder mappings...** in Preferences. Clear **Use platform defaults from the Stuff folder** to edit your own table. Add one row per shared location. The left column is the absolute prefix stored in the project and the right column is its location on this machine. **OK** saves the entire table. An empty personal table disables mapping. Select the defaults checkbox again to inherit future changes from the studio. Restart PPM to apply the table.

Administrators can put `defaultusersettings_windows.ini` and `defaultusersettings_macos.ini` in Stuff. For example, the Mac file can contain:

```ini
[PathMappings]
size=1
1\stored=//fileserver/anime
1\local=/Volumes/anime
```

Use forward slashes in the values. Server and share names are stored in plain text. Match complete directory prefixes, ignoring case and slash direction. The longest stored prefix wins when opening a project. When saving an edited field, PPM asks you to choose if several stored paths are possible.

## Saving the settings

The fields in the main Preferences dialog are saved when you finish entering it (when you move from the field to somewhere else).
The **OK** button only closes the dialog.

On Windows, the preferences are saved in the following file.

```
%LOCALAPPDATA%\PPM\usersettings.ini
```

(This is usually `C:\Users\<user name>\AppData\Local\PPM\usersettings.ini`.)

On macOS, the file is `~/Library/Application Support/PPM/usersettings.ini`, where `~` is your home folder. It is separate from the app bundle and the Stuff folder.

For the application and language settings you have never changed, the contents of `defaultusersettings.ini` in the Stuff folder are used as the initial values.
See [The Stuff Folder](../administration/stuff-folder.md) for details.

## States recorded automatically

In addition to the items in the Preferences dialog, the following states are recorded in the same file and kept the next time you start PPM.

- The last project you opened
- For each project: the checked workflows, the workflow groups expanded in the list view, the workflow lock state, and the selected episode
- Whether you use the list view or the flowchart view
- The position and size of the window, and the position of the border between the left and right parts
- Which columns of the scene list are shown, and the thumbnail size

---

[← Back to contents](../index.md)
