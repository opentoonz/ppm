[日本語](troubleshooting.ja.md)

# Troubleshooting and FAQ

This page helps you find what to do from the symptom.
The details are on the pages linked from each item.

First, try the following. In many cases, this shows the cause.

- **Press Refresh**: Scenes that someone else moved forward, and rows that turned gray after you ran actions, are updated to their current state.
- **Look up the scene with the scene search**: The workflow group the scene belongs to, the workflows it is in, its take, its scene info tags, and its log are shown together (see [Main Window](using/main-window.md)).
- **Look at the files with Open Containing Folder**: PPM decides the progress from the names and locations of files. Seeing which files are in the scene folder tells you why PPM shows what it shows.

## When starting PPM

| Symptom | Cause and what to do |
|---|---|
| The **Project** list is empty, and nothing is shown | PPM cannot find the Stuff folder. Check `StuffFolderPath` in `conf.ini`, which is in the same folder as `ppm.exe`. If you use backslashes (`\`), write each one twice, like `\\` (see [Installation](getting-started/installation.md)). If the Stuff folder is in a shared folder, also check that you can open that folder in Explorer. |
| The project you want is not in the **Project** list | Check that the project file (`.ppmprj`) is in `projects` in the Stuff folder. A project that someone else has just created appears in the list after you restart PPM. |
| "Warning: Translation file not found" is shown, and the screen is in English | The translation file for the language chosen in Preferences is not in the `loc` folder of the Stuff folder. Check that the file is at the path shown in the warning (see [UI Language and Translation Files](administration/translations.md)). |
| A warning such as "Warning: TVPaint binary not found" is shown | Choose the installed application in Preferences, or clear the path to use the system default for that extension. See [Preferences](using/preferences.md). |

## A scene does not appear in the list

The conditions for a scene to appear in the list, and the order to check them, are described in [Which Scenes Appear in the List](using/scene-list.md).
The common causes are as follows.

| Symptom | Cause and what to do |
|---|---|
| The scene is not listed in any workflow | The scene does not belong to any workflow group. Check "Main Flow Group" in the **scene search**. If it is empty, register the scene (see [Registering Scenes](using/registering-scenes.md)). |
| The scene search shows "The searched scene was not found." | The scene belongs to no workflow group and has no log. Also check the scene number for typing mistakes. |
| You placed or renamed a file by hand, but the scene is not listed | Check that the file name matches the condition path of the workflow (hover the mouse over the workflow to see its condition paths). If it does, press the reload button (circular arrow icon) of the workflow group (see [Working with Files Directly](concepts/working-with-files.md)). |
| A scene that someone else moved forward is still in the previous workflow | Press **Refresh**. Other people's operations are not shown automatically. |
| The same scene is listed in two workflows | The cue file of the previous workflow is left. Open the scene folder, and delete or rename the file that is not needed. |
| A scene is not listed in a project that uses episodes | Check that the correct episode is selected in **Episode** at the top. |
| Nothing is listed | Check that the checkbox of the workflow is on, on the left (see [Main Window](using/main-window.md)). |
| Scenes stopped appearing after the project settings were changed during production | When you change condition paths or folder locations, the existing files can no longer be found (see [Changing the Design During Production](designing/changing-in-production.md)). |
| Thumbnails are not shown | Check that the images are where the "Scene Thumbnail Path" in the project settings says (see [Basics](designing/basics.md)). |

## A file does not open

| Symptom | Cause and what to do |
|---|---|
| Pressing **Open** does not start the application | Check the application path in Preferences. If it is empty, double-click the file in Explorer or Finder to check its default application. If a configured application cannot start, PPM shows its path and the file it tried to open. See [Preferences](using/preferences.md). |
| The file does not open in OpenToonz | If **OpenToonz arguments** is set, check that it is a valid startup option of OpenToonz. If the file opens after you clear it, the arguments are the cause. |
| Pressing **Open** opens the scene folder instead of a file | If the cue file is not a `.clip`, `.tvpp`, or `.tnz` file (for example, a text file used as a marker), opening the scene folder is the expected behavior (see [Working on a Scene](using/scene-operations.md)). |
| **Open XDTS** is disabled | The timesheet is not found. PPM looks for a file that matches the "XDTS File Name" of the workflow group, in the standard scene folder. Check the name and location of the file (see [Workflow Groups](designing/workflow-groups.md)). |
| **View Log** is disabled | The scene has no log yet. No action or registration has been done for the scene. |
| The scene folder does not open | Check in Explorer that you can connect to the shared folder. |

## An action fails

When a step of an action fails partway, the following warning is shown.

> The action was failed or interrupted in the following scene(s). Click "Show Details" for the log.
> You may need to check and modify files manually.

Do the following.

1. Press **Show Details...** to see which step of which scene stopped.
2. Press **Open containing folder(s)** to check the state of the files.
   The steps that were run before the failure are not reverted.
3. Remove the cause, then fix the files by hand, or press **Refresh** and run the action again.

The failure is recorded in the log of the scene with `[FAILED]`.
The contents of **Show Details...** are not kept in the log, so note them down if needed.
How this works is described in "When an action fails" in [How Scenes Flow Through Workflows](concepts/scene-flow.md).

The common causes are as follows.

| Cause | What to do |
|---|---|
| The file is still open in the application for your work | Save and close the file, then try again (see [Working with Files Directly](concepts/working-with-files.md)). |
| A file with the same name already exists at the rename or copy destination | Check the scene folder for old files that are left. |
| Someone else moved the same scene forward first | Press **Refresh**, and check which workflow the scene is in now (see [Running PPM on a Shared Folder](administration/shared-operation.md)). |
| You cannot connect to the shared folder, or you have no write permission | Check in Explorer that you can create a file in that folder. Ask the administrator about permissions (see "Access rights" in [Running PPM on a Shared Folder](administration/shared-operation.md)). |
| The source file or folder of a copy does not exist | Before pressing **OK**, check the confirmation dialog for steps with a warning icon. |

### When undoing fails

When **Undo** fails partway, a similar warning is shown ("Undoing was failed or interrupted.").
It is recorded in the log as `[UNDO FAILED]`.
As with a failed action, open the scene folder and fix the files by hand.

### When the log or scene info tags cannot be written

When "Failed to access the log folder ..." or "Failed to open the log file ..." is shown, the log folder cannot be written to.
The file operations may be done, but the log may not be recorded.
Check that you can open the folder and that you have write permission (see "Access rights" in [Running PPM on a Shared Folder](administration/shared-operation.md)).

When "Failed to access the scene info folder ..." or "Failed to write the scene info file ..." is shown, the scene info tag cannot be written.
The path shown is in the `scene_info` folder in the project info folder. Check the write permission, as with the log folder.

### PPM stops for a while

When PPM rewrites the scene list of each workflow, it creates a `..._details.lock` file so that two people do not rewrite it at the same time.
If this file is left, for example because PPM on another PC was forced to quit, PPM waits up to about 30 seconds before it continues.
If PPM stops often, check the `progress_cache` folder in the project info folder for old `.lock` files (see [Running PPM on a Shared Folder](administration/shared-operation.md)).

## The Google integrations do not work

| Symptom | Cause and what to do |
|---|---|
| The **Undo** button changed to "Undo (*)" | Writing to the Google Spreadsheet failed. Check the cause in the records left in the `google_spreadsheet_cues` folder of the log folder, and fix the spreadsheet by hand if needed (see [Google Spreadsheet Integration](integrations/google-spreadsheet.md)). |
| Nothing is written to the spreadsheet | When the **Google Apps Script URL** in the project settings is empty, this step is skipped. Nothing is written for scenes that have no row in the sheet, either. |
| "Failed to send the Google Chat notification" is shown | Check the cause shown in the warning, and `google_chat/google_chat.log` in the log folder (see [Google Chat Notification](integrations/google-chat.md)). |
| When you quit, a warning says that writing or notifications are not finished | Press **Wait**, and quit after a while. If you press **Quit anyway**, the data that has not been sent is lost (see [Main Window](using/main-window.md)). |

## FAQ

### I ran an action by mistake

Press the **Undo** button in the row that turned gray to revert it.
The **Undo** button disappears when you press **Refresh**, so after that, fix the files by hand (see [Working on a Scene](using/scene-operations.md) and [Working with Files Directly](concepts/working-with-files.md)).

### I want to start a scene from a workflow in the middle, or send it back to an earlier workflow

To start from a workflow in the middle, right-click the workflow and use **Register a scene starting from this workflow** (see [Registering Scenes](using/registering-scenes.md)).
To send a scene back to an earlier workflow in the same workflow group, you can use a rewind (see [Working on a Scene](using/scene-operations.md)).
You can also fix it by placing or renaming the cue file by hand (see [Working with Files Directly](concepts/working-with-files.md)).

### Can I move files without using PPM?

Yes. PPM decides the progress from the names and locations of files.
However, after you move files, press the reload button of the workflow group (see [Working with Files Directly](concepts/working-with-files.md)).

### Project settings changed by someone else do not show in my PPM

PPM reads the project file only when it starts and when you switch projects.
Restart PPM (see [Running PPM on a Shared Folder](administration/shared-operation.md)).

### The user name in the log is different from the user name in Preferences

The log records the Windows user name.
The **User Name** in Preferences is used for `[USER_NAME]` in the project settings (see "Tracing what happened from the log" in [Backup and Recovery](administration/backup.md)).

### The log has both Japanese and English

The log is written in the language of the PPM of the person who did the operation.
Lines written by a Japanese PPM are recorded like `[失敗]` or `2026年 10月30日  10時12分05秒`.

### The file of the scene list of each workflow is broken or lost

Press the reload button of the workflow group to rebuild it from the cue files (see [Backup and Recovery](administration/backup.md)).

### How do I uninstall PPM?

Delete the folder you extracted.
To also delete the settings of each user, delete `%LOCALAPPDATA%\PPM\usersettings.ini` as well (see [Installation](getting-started/installation.md)).

## If the problem is not solved

If you think it is a bug, report it as described in [Reporting Bugs and Requests](about/reporting-issues.md).

---

[← Back to contents](index.md)
