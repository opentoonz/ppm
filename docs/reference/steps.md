[日本語](steps.ja.md)

# Steps

This page lists the types of action steps and their settings.
For how to add and reorder steps, see [Designing Workflows](../designing/workflows.md).

## Common to all steps

- **Order**: When an action runs, its steps run from top to bottom. If a step fails, PPM stops there (see [How Scenes Flow Through Workflows](../concepts/scene-flow.md)).
- **Confirmation dialog**: The action confirmation dialog shows what each step will do, with the actual paths.
- **Undo**: Pressing **Undo** reverts the steps in reverse order. Some steps cannot be reverted (see each step below).
- **Paths**: Path fields accept macros (`[ROOT]`, `[CUTNUMBER]`, path macros, and so on; see [Macros](macros.md)). In path fields, typing `[` shows the macros you can use as candidates, and known macros are shown in color.
- **Suggest...**: Fields that take a path to the next workflow (such as **Destination Paths** of **Rename**) have this button. Pressing it shows the condition paths of the next workflows as candidates.
- **Cue file**: On this page, "cue file" means the file that matched the condition path of the workflow where the action is run (see [Progress Is Managed by Files](../concepts/progress-files.md)).

## File operations

### <img src="../images/icons/ppm64.png" width="24" height="24" alt=""> Rename

Renames the cue file (it can also move it to another folder).
This is the step most often used to move a check workflow forward.

| Item | Contents |
|---|---|
| **Destination Paths** | The new name (path). You can enter several. |

- If you enter several destination paths, the file is copied to each of them, and then the original file is deleted.
- If a file with the same name already exists, a message asks whether to overwrite it.
- **Undo**: Deletes the created files and restores the original name. If the contents of the file have changed since it was renamed, a warning is shown in the confirmation dialog.

### <img src="../images/icons/ppm64.png" width="24" height="24" alt=""> Copy

Copies the cue file. The original file is kept.
Used, for example, to pass a file to the scene folder of the next workflow group.

| Item | Contents |
|---|---|
| **Destination Paths** | The destination paths. You can enter several. |

- If the cue file is a folder, it is copied with its contents.
- **Undo**: Deletes the copied files. When a folder was copied, files and folders that already existed in the destination are not deleted. Files overwritten by the copy cannot be restored.

### <img src="../images/icons/renameFile64.svg" width="24" height="24" alt=""> <img src="../images/icons/copyFile64.svg" width="24" height="24" alt=""> Rename File, Copy File

Renames or copies a file other than the cue file.

| Item | Contents |
|---|---|
| **Source Path** | The path of the file to rename (copy). |
| **Destination Paths** | The new paths. You can enter several. |

The behavior and undo are the same as **Rename** and **Copy**.

### <img src="../images/icons/copyFolder64.svg" width="24" height="24" alt=""> Copy Folder Contents

Copies (or moves) selected files in a folder.
Used, for example, to pass a set of key animation files to the in-between folder, or to gather exported images into the ink-and-paint folder.

| Item | Contents |
|---|---|
| **Source Path** | The folder that contains the files to copy. If empty, the folder of the cue file is used. |
| **Destination Paths** | The destination folders. You can enter several. `[CURRENT_DATE]` (the date of execution, in the form `261030`) can also be used. |
| **Mode** | The items below. |

If **Source Path** is just `[SKIP_CUE_FILE]`, files are copied from the folder of the cue file, but the cue file itself is not copied.
Use this, for example, when the cue file is a marker text file that is not part of the production data.

**Target Selection**

| Item | Contents |
|---|---|
| **Default Selection** | **All selected**: The file list in the confirmation dialog starts with everything selected.<br>**None selected**: It starts with nothing selected.<br>**All, without selection screen**: Everything is copied without showing the file list. |
| **Folder Depth** | How many levels the file list in the confirmation dialog shows. If empty, only one level (directly under the source folder) is shown. |
| **Whitelist / Blacklist** | Names to select from the start, prefixed with `+`, and names to deselect, prefixed with `-`, separated by semicolons (`;`) (e.g. `+genga;-memo.txt`). |

Even with **None selected**, the cue file, the timesheet (XDTS) set for the workflow group, and its accompanying folder (`..._xdts`) are selected from the start.

**Transfer**

| Item | Contents |
|---|---|
| **Transfer** | **Copy**: Keeps the original files.<br>**Move**: Deletes the original files after copying. |

**Name Conversion**

| Item | Contents |
|---|---|
| **Rename Rule** | Renames files with a rename script (see [Rename Scripts](../integrations/rename-scripts.md)). |
| **Take Suffix** | Replaces the take mark in folder names. It works only when the scene is at take 2 or later.<br>**To current take**: Replaces the mark of the previous take with that of the current take.<br>**To shooting take**: Replaces the mark of the current take with the mark for shooting (starting with `_r`). |
| **Flatten subfolders** | Places all files in subfolders directly under the destination folder. |

When several are selected, they are applied in this order: rename rule, take suffix, flattening subfolders.

**Others**

| Item | Contents |
|---|---|
| **Images** (Convert to JPEG) | Converts image files to JPEG when copying. Cannot be combined with **Move** (because the original images could not be restored). |
| **If File Exists** | What to do when a file with the same name exists in the destination.<br>**Ask**: Choose overwrite, skip, or cancel when copying.<br>**Overwrite**: Overwrites without confirmation.<br>**Skip**: Keeps the existing file without confirmation.<br>**Overwrite if newer**: Overwrites only if the source file is newer.<br>Files that are not overwritten stay where they are, even with **Move**. |

- **Undo**: Deletes the copied files. With **Move**, the files are returned to their original location.

### <img src="../images/icons/cue64.svg" width="24" height="24" alt=""> Create Cue File

Creates a text file.
Used to place a cue file (marker file) for the next workflow.

| Item | Contents |
|---|---|
| **File Path** | The path of the file to create. |
| **Content (optional)** | Text written into the file, such as instructions for the artist. If empty, the file is empty. |

- If the folder for the file does not exist, a message asks whether to create it.
- If a file with the same name already exists, the step is marked with a warning icon in the confirmation dialog, and a message asks whether to overwrite it when the step runs.
- **Undo**: Deletes the created file. If an existing file with the same name was overwritten, it is not deleted but restored to its content before it was overwritten.

### <img src="../images/icons/delete64.svg" width="24" height="24" alt=""> Ask Delete

Lets the person running the action choose, in the confirmation dialog, whether to delete a file.

| Item | Contents |
|---|---|
| **Delete By Default** | When on, "Delete" is selected in the confirmation dialog from the start. |
| **Path (optional)** | The path of the file or folder to delete. If empty, the cue file. |

- If the file does not exist, this step is skipped.
- A folder can be deleted only when it is empty.
- **Undo**: Not possible. Deleted files are not restored.

### <img src="../images/icons/new-folder.svg" width="24" height="24" alt=""> Create Empty Folder

Creates an empty folder.

| Item | Contents |
|---|---|
| **Folder Path** | The path of the folder to create. `[CURRENT_DATE]` (the date of execution, in the form `261030`) can also be used. |

- If the folder already exists, nothing is done.
- **Undo**: Deletes the folder created by this step, only if it is empty.

### <img src="../images/icons/zip-folder.svg" width="24" height="24" alt=""> Zip Folder

Packs a folder into a ZIP file.

| Item | Contents |
|---|---|
| **Source Folder (optional)** | The folder to pack. If empty, the folder of the cue file. |
| **Destination Zip Path** | The path of the ZIP file to create. |

- The destination ZIP may be inside the source folder. On macOS, PPM skips the output archive and files or folders whose names start with a dot (such as `.DS_Store`). Entry names use UTF-8.
- If a ZIP file with the same name exists, a message asks whether to overwrite it. If the destination folder does not exist, a message asks whether to create it.
- **Undo**: Deletes the created ZIP file.

## Progress management

### <img src="../images/icons/moveFlow64.png" width="24" height="24" alt=""> Move Flow Group

Moves the scene to another workflow group (see [How Scenes Flow Through Workflows](../concepts/scene-flow.md)).

| Item | Contents |
|---|---|
| **Flow Group** | The workflow group to move to. |

- After the move, cue files are looked for only in the workflows of the new workflow group.
- **Undo**: Moves the scene back to the original workflow group.

### <img src="../images/icons/start_branch64.png" width="24" height="24" alt=""> <img src="../images/icons/end_branch64.png" width="24" height="24" alt=""> Start Branch Flow, End Branch Flow

Lists the scene in the workflows of another workflow group as well, while it keeps its membership (branch flow), or stops doing so.

| Item | Contents |
|---|---|
| **Flow Group** | The workflow group to add (remove) as a branch flow. |

- **Undo**: Removes the started branch flow (restores the ended branch flow).

### <img src="../images/icons/setInfo64.svg" width="24" height="24" alt=""> Set Scene Info Tag

Sets a scene info tag on the scene (see [Scene Info Tags](../designing/info-tags.md)).

| Item | Contents |
|---|---|
| **Info Name** | Select the scene info tag to set from the ID names of the project's scene info tags (e.g. `BG_ONLY`). If an ID name that is not in the project is set, it is shown with "(not defined)". |
| **Value** | The value to set. Write `1` for Bool, the value for Enum (e.g. `WIP`), and the string itself for String. |

- If **Value** is empty, the scene info tag is removed. If the scene does not have the tag, nothing happens.
- **Undo**: Restores the state before the action.

### <img src="../images/icons/ppm64.png" width="24" height="24" alt=""> Add Next Workflow

Adds a workflow to the next workflows of the action.
Use it inside a conditional branch step to add the workflow only when the condition is met (see [Adding Next Workflows Conditionally](../integrations/add-progress.md)).

| Item | Contents |
|---|---|
| **Workflow** | The workflow to add. |

- **Undo**: Removes the scene from the added workflow.

### <img src="../images/icons/ppm64.png" width="24" height="24" alt=""> Release Progress

Removes the scene from the record of the specified workflow.
It can remove the scene not only from the next workflows of the action, but also from next workflows set by earlier workflows (other workflows the scene is in now).

| Item | Contents |
|---|---|
| **Workflow** | The workflow to remove the scene from. |

- If the scene is not in that workflow, nothing is done.
- Only the record is changed. Cue files are not deleted. If the cue file remains, the scene appears in that workflow again when the reload button of the workflow group is pressed.
- **Undo**: Restores the scene only if it was actually removed when the action ran.

## Checks and branches

### <img src="../images/icons/check_ok64.svg" width="24" height="24" alt=""> Check Existence

Checks in the confirmation dialog whether files exist (or do not exist).
Used to avoid moving a scene forward when a file has not been made.

| Item | Contents |
|---|---|
| **Mode** | **EXISTENCE**: Warns if a file does not exist.<br>**ABSENCE**: Warns if a file exists. |
| **Check a Scene Info Tag instead of a file (SCENEINFO)** | When on, checks whether scene info tags are set, instead of files. |
| **Targets** | The paths of the files to check (or the ID names of the scene info tags, with SCENEINFO). You can enter several. |

- The confirmation dialog shows the results as `[OK]`, `[MISSING]`, or `[EXISTS]`, with a warning icon if the condition is not met.
- Even with a warning, you can still press **OK** to run the action. This step does not stop the action.
- Nothing is checked on undo.

### <img src="../images/icons/branch_info64.svg" width="24" height="24" alt=""> Conditional Branch By Info Tag

Changes which step runs, depending on the value of a scene info tag.

| Item | Contents |
|---|---|
| **Info Name** | The scene info tag to check. |
| **Condition Value** | Checks whether the value of the scene info tag equals this value. Write `1` for Bool, the value for Enum (e.g. `WIP`), and the string itself for String. If empty, checks whether a Bool tag is true, or whether any other tag is set. |
| **If True** | The step that runs if the condition is met. |
| **If False** | The step that runs if the condition is not met (including when the tag is not set). |

### <img src="../images/icons/branch_file64.svg" width="24" height="24" alt=""> Conditional Branch By File Existence

Changes which step runs, depending on whether a file exists.

| Item | Contents |
|---|---|
| **Condition Path** | The path of the file or folder to check. |
| **If Exists** | The step that runs if it exists. |
| **If Not Exists** | The step that runs if it does not exist. |

Common to both conditional branch steps:

- Each case can hold one step. Choose **None** to do nothing. To do two or more things, line up as many conditional branches with the same condition as you need.
- The condition is judged when the confirmation dialog opens.

## Integrations

### <img src="../images/icons/googleSheets64.png" width="24" height="24" alt=""> Input Google Spreadsheet

Writes a value into a scene sheet in Google Spreadsheet.
For the setup and behavior, see [Google Spreadsheet Integration](../integrations/google-spreadsheet.md).

| Item | Contents |
|---|---|
| **Sheet Name (optional)** | The name of the sheet to write into. |
| **Flow Name** | The workflow name of the column to write into. |
| **Value** | The value to write. |

### <img src="../images/icons/googleChat64.png" width="24" height="24" alt=""> Notify Google Chat

Sends a notification to a Google Chat space.
For the setup and behavior, see [Google Chat Notification](../integrations/google-chat.md).

| Item | Contents |
|---|---|
| **Destination** | Where to send it. |
| **Message** | The message to send. |
| **Allow Free Text** | Lets the user add to the message in the confirmation dialog. |
| **Notify On Cancel** | Sends a cancel notification when the action is undone. |
| **Cancel Message (optional)** | The message sent when the action is undone. |
| **Thread Key (optional)** | A key for grouping messages into the same thread. |

---

[← Back to contents](../index.md)
