[日本語](basics.ja.md)

# Basics

In the **Basics** category, you set up things that concern the whole project.

![The Basics page](../images/ja/designing/basics.png)
<!-- English screenshot to be made: images/en/designing/basics.png -->

## Items

| Item | Contents |
|---|---|
| **Project Title** | The title of the project. Shown in the title of the dialog and elsewhere. It is not the same as the file name of the project (the project ID). |
| **Description** | A description of the project. Write anything you like. |
| **Scene Number Prefix [PREFIX]** | A string added before the scene number (see below). |
| **Project Root Folder [ROOT]** | The folder for the scene files. |
| **Project Info Folder** | The folder for the workflow group membership, the list of scenes in each workflow, and the scene info tags. |
| **Log Root Folder** | The folder for the log of each scene (the log folder). |
| **Google Apps Script URL** | The URL of the Google Apps Script used for the Google Spreadsheet integration and Google Chat notifications (see below). |
| **Google Chat Destinations** | The names of the destinations of Google Chat notifications (see below). |
| **Scene Thumbnail Path** | The path of the thumbnail image shown in the scene list (see below). |
| **Group Scenes by 100 [SEQUENCE]** | Whether to manage scenes in groups of 100 (see below). |
| **Has Episodes** | Turn this on for a title made of several episodes, such as a TV series (see below). |

## Scene number prefix [PREFIX]

A string added before the scene number.
In the sample project it is `c`, so the names of the files and folders of scene `0001` start with `c0001`.

In a path, `[PREFIX]` is replaced with this string.
It is also used in the names of the scene info file (`c0001.ini`) and the log file (`c0001.md`).

If you do not use a prefix, leave it empty.

## Root, project info, and log folders

The roles of the three folders are explained in [Progress Is Managed by Files](../concepts/progress-files.md).
Press the button on the right of a folder field to choose the folder.

A **relative path** is treated as a path relative to the Stuff folder.
For example, `sample_data` means `[Stuff folder]/sample_data`.
With shared Stuff, prefer relative paths. For an absolute path, the field shows the location used on this machine. Hover over the field to see the stored path or a warning if no mapping matches. You can enter either form. Saving another setting preserves the original text of untouched path fields. An unmatched new absolute path can be saved after confirmation. See [Running PPM on a Shared Folder](../administration/shared-operation.md).

In a path, `[ROOT]` is replaced with the root folder.
Usually you write the locations of scene folders, which you decide in [Path Macros](path-macros.md), in the form `[ROOT]/...`.

## Episodes [EPISODE]

For a title made of several episodes, such as a TV series, turn on **Has Episodes** and enter the **Episode Names**.
Separate the episode names with commas (`,`) or semicolons (`;`) (e.g. `01, 02, 03`).

With episodes:

- A combo box for switching episodes is shown in the main window (see [Main Window](../using/main-window.md)).
- `[EPISODE]` in a path is replaced with the name of the episode selected in the main window. Put `[EPISODE]` in the scene folder paths to divide the folders by episode.
- A folder for each episode is created in the project info folder and the log folder, and the records are saved in it.

## Group scenes by 100 [SEQUENCE]

When this is on, scenes are managed in groups of 100 scenes (**sequences**).

- `[SEQUENCE]` in a path becomes the leading number of the scene number without its last two digits (e.g. `0123` → `01`, `0045` → `00`).
- Scene info and logs are saved in a folder for each sequence (`00`, `01`, ...).
- Scene names must begin with a number of 3 or more digits (`0123`, `0123b`, and so on).

When this is off:

- `[SEQUENCE]` cannot be used (it is replaced with an empty string, and the `/` before and after it are merged into one).
- Scene info and logs are saved without sequence folders.
- Scene names can be anything.

> **Note**: Do not change this setting in the middle of production.
> PPM does not reorganize the existing scene info and log folders.
> If you change it, the existing scene info (including the take records) and logs will no longer be found.
> When you try to save after changing it, a confirmation message is shown (see [Changing the Design During Production](changing-in-production.md)).

## Scene thumbnail path

The path of the image shown in the "Thumbnail" column of the scene list.
Write it with `[ROOT]`, `[PREFIX]`, `[SEQUENCE]`, `[CUTNUMBER]`, path macros, and so on (e.g. `[ROOT]/thumbnails/[PREFIX][CUTNUMBER].jpg`).

If you leave it empty, the Thumbnail column is not shown in the scene list.

## Google Apps Script URL and Google Chat destinations

These are used to write progress into a Google Spreadsheet and to send notifications to Google Chat.
If you do not use them, leave them empty.

- **Google Apps Script URL**: Anyone who knows this URL can use the script, so it is not shown on the screen and is saved in an obfuscated form. To change it, select all (Ctrl+A) in the field and enter the new URL.
- **Google Chat Destinations**: The names of the destinations that can be chosen in the "Notify Google Chat" step, separated by commas or semicolons. **Get from Script** loads the names registered in the script. **Send Test Message** posts a test message.

The setup is explained in [Google Spreadsheet Integration](../integrations/google-spreadsheet.md) and [Google Chat Notification](../integrations/google-chat.md).

---

[← Back to contents](../index.md)
