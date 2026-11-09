[日本語](initial-setup.ja.md)

# Initial Setup

When you start PPM for the first time, set the language and the applications for your work in **Preferences**.
Preferences are saved for each user, so each staff member only needs to do this once on their own computer.

On Windows, choose **Preferences** from the gear menu. On macOS, open settings from the **PPM** application menu or press **Command+,**.

![the Preferences dialog](../images/ja/getting-started/preferences.png)
<!-- English screenshot to be made: images/en/getting-started/preferences.png -->

> On Windows, if PPM starts in Japanese, the gear button menu has three items: プロジェクト設定 (Project Settings), 環境設定 (Preferences), and PPM について (About PPM).
> Choose the second one, 環境設定, and select "English" in 言語 (Language).

> For administrators: by placing `defaultusersettings.ini` in the Stuff folder, you can set the initial values of the preferences for everyone.
> See [The Stuff Folder](../administration/stuff-folder.md) for details.

## Choosing the language

In **Language**, choose the language of the PPM user interface.
At first, PPM uses the system display language (English if there is no translation file for that language).
The change takes effect the next time PPM is started.

The available languages are those of the translation files in the `loc` folder of the Stuff folder.
Choose "English" to show the user interface in English without a translation.

## Setting the application paths

In the PPM list, you can open the file of each scene directly with the **Open** button.
Press the **...** button beside each file type and choose its application. Select the executable (`.exe`) on Windows or the application bundle (`.app`) on macOS.

| Setting | Files to open |
|---|---|
| TVPaint executable path | TVPaint projects (`.tvpp`) |
| Clip Studio executable path | CLIP STUDIO PAINT files (`.clip`) |
| OpenToonz executable path | OpenToonz scene files (`.tnz`) |
| XDTS application path<br>(such as XDTS Viewer or Toei Animation Digital Exposure Sheet) | Timesheet files (`.xdts`) |

If a path is **left empty**, the operating system opens the file with its default application for that extension.
For example, if you already open `.clip` files with CLIP STUDIO PAINT, you can leave the Clip Studio path empty.

In **OpenToonz arguments**, you can write startup options added when OpenToonz opens a `.tnz` file.
They are not used if the OpenToonz path is empty.

If no file is found at a path you set, a warning is shown when PPM starts.
If the installation folder of an application changes (for example, after an upgrade), set the path again.

## User name

**User Name [USER_NAME]** is the name substituted for `[USER_NAME]` in the project settings.
It matters, for example, when `[USER_NAME]` is used in the path of a personal work folder, or when you want to record the name of the person in charge on the scene sheet.

If you leave it empty, PPM uses your login name (shown faintly in the field).
Enter a name only if you want to use a different name.
Follow the instructions of your pipeline designer on what name to enter.

> The operation log of a scene always records the login name.

---

[← Back to contents](../index.md)
