[日本語](main-window.ja.md)

# Main Window

This page explains each part of the main window, which appears when you start PPM, and how to use it.

## Layout of the window

The main window is divided into left and right parts.

![The whole main window](../images/ja/home/main-window.png)
<!-- English screenshot to be made: images/en/home/main-window.png -->
![Layout of the main window](../images/ja/using/main-window-areas.png)
<!-- English screenshot to be made: images/en/using/main-window-areas.png -->

1. **Top buttons** (top left): Buttons for switching projects, settings, Refresh, Register, Retake, and scene search.
2. **Workflow selection** (left): Choose which workflows' scenes to show in the list. You can switch between the list view and the flowchart view.
3. **Scene list** (right): The scenes in the selected workflows are listed. From here, you open files and run actions.

Drag the border between the left and right parts to change the width of the left part.
The top buttons are rearranged into one to four rows to fit the width of the left part.
The width of the left part and the display state are kept the next time you start PPM.

## Top buttons

| Button | What it does |
|---|---|
| **Switch between list view and flowchart view** (arrow icon) | Switches the workflow selection between the list view and the flowchart view. |
| **Project** | Selects the project to use. The projects in the `projects` folder of the Stuff folder are listed. |
| **Project Settings / Preferences** (Windows gear icon) | Opens **Project Settings**, **Preferences**, or **About PPM** from the menu. |
| **Episode** | Selects the episode to work on. Shown only in projects that use episodes. |
| **Refresh** | Rebuilds the scene list (see [Which Scenes Appear in the List](scene-list.md)). |
| **Register** | Registers scenes to a workflow group, and sets or removes scene info tags (see [Registering Scenes](registering-scenes.md)). |
| **Retake** | Issues retakes for scenes (see [Retakes](retakes.md)). |
| **Scene Number** / **Search Scene** (magnifier icon) | Shows the current state of a scene (see "Scene search" below). |

On macOS, the **PPM** menu contains About PPM and settings. Choose a page under **Project → Project Settings** to open that page directly. You can also switch pages in the dialog’s sidebar.

### Switching episodes

In a project that uses episodes, select the episode to work on in **Episode**.
If there are two or more episodes, select **All Episodes** to list the scenes of all episodes together.
In this case, the **Episode** column is shown in the list.

The selected episode is recorded for each project and kept the next time you start PPM.

## Selecting workflows

On the left, turn on the checkboxes of the workflows you want to list.
The scenes in the checked workflows are listed together on the right.
The checked state is recorded for each project.

Right-click a workflow to choose **Register a scene starting from this workflow** (see [Registering Scenes](registering-scenes.md)).
Hover the mouse over a workflow to see its condition paths (**Conditional file(s)**).

### List view

Workflows are listed vertically for each workflow group.

<!-- Image: images/en/using/list-view.png (the left part in the list view, showing the workflow group headers, the colored workflow checkboxes, and the lock icons) -->

The header of each workflow group has the following:

- **Triangle icon**: Expands or collapses the workflow group.
- **Checkbox with the workflow group name**: Turns all the workflows in the group on or off. The number next to the name is the number of scenes that belong to the workflow group.
- **Reload button** (circular arrow icon): Looks for the files of the workflow group again and rebuilds the scene list of each workflow (see [Which Scenes Appear in the List](scene-list.md)). Hover the mouse over it to see a description.

The **lock icon** at the right end of each workflow **locks** the actions of that workflow (see [Working on a Scene](scene-operations.md)).

### Flowchart view

The connections of the workflows made in the project settings are shown as a flowchart.
The tutorial uses this view.

![The flowchart view](../images/ja/getting-started/tutorial-open-project.png)
<!-- English screenshot to be made: images/en/getting-started/tutorial-open-project.png -->

- **Workflow group frame**: The checkbox at the top left turns all the workflows in the frame on or off. At the top right are the number of scenes that belong to the workflow group (such as "3cuts") and the reload button (circular arrow icon).
- **Workflow box**: The checkbox selects whether to list the scenes in that workflow. The white number is the number of scenes in that workflow. Hover the mouse over the number to see the list of scene names. The lock icon at the far right locks the workflow (the same as in the list view).
- **Arrows**: Show the workflows that a scene moves to when an action is run.

Use the mouse wheel to zoom in and out, and drag with the middle mouse button held down to move the view.

#### Role buttons

The colored circles at the bottom right of the flowchart are **role buttons**.
Each role defined in the project settings, such as "episode director" or "director", is assigned a color.

Press a role button to turn on or off all the workflows that have the same color as the role.
For example, pressing the episode director role button turns on all the episode director check workflows of LO, key animation, and so on.
The mark on a button shows the checked state of the workflows of that color (✓: all on, -: some on, no mark: all off).

![The role buttons](../images/ja/using/role-buttons.png)
<!-- English screenshot to be made: images/en/using/role-buttons.png -->

## Scene list

On the right, the scenes in the selected workflows are listed, one per row.

![The scene list](../images/ja/using/scene-list-table.png)
<!-- English screenshot to be made: images/en/using/scene-list-table.png -->

| Column | Contents |
|---|---|
| (Checkbox) | Selects scenes to run actions on together. Shown in the rows of workflows that have only one action (see [Working on a Scene](scene-operations.md)). |
| **Open** | The button to open the file, and the buttons for the timesheet, the scene folder, and the log. |
| **Workflow Name** | The workflow the scene is in. |
| **Media** | The media of the protocol (Digital / Paper / Other). |
| **Thumbnail** | The thumbnail image of the scene. Shown only if the location of thumbnails is set in the project settings. |
| **Episode** | Shown only in projects that use episodes. |
| **Scene Number** | The scene name, shown with the take number or the rewind mark (`_t02`, `_r`, and so on). |
| **Last Modified Date** | The date and time the cue file was last modified. Shows the time for today, the month and day for this year, and the full date for earlier years. |
| **Action** | The buttons that move the scene forward. The rewind button is also here. |
| **Information** | The labels of the scene info tags attached to the scene (see [Scene Info Tags](scene-info.md)). |

The list is sorted by workflow, episode, media, and scene name.
Click a column header to sort by that column.

### Showing columns and the thumbnail size

Right-click a column header to show or hide columns.
The **Media**, **Thumbnail**, **Episode**, **Last Modified Date**, and **Information** columns can be hidden.
The **Open**, **Workflow Name**, **Scene Number**, and **Action** columns are always shown.

In projects that use thumbnails, use **Thumbnail Size** in the same menu to switch between **Small** and **Large**.

## Scene search

Enter a scene name in the **Scene Number** field at the top and press Enter or the magnifier button to show the current state of the scene on the right.

![Result of a scene search](../images/ja/using/scene-search.png)
<!-- English screenshot to be made: images/en/using/scene-search.png -->

| Item | Contents |
|---|---|
| **Main Flow Group** | The workflow group the scene belongs to (the main flow). |
| **Branch Flow Group** | The workflow groups the scene has been added to as branch flows. |
| **Workflow** | The workflows the scene is in. If it is in several workflows at the same time, all of them are shown. |
| **Current Take** | The current take number. For a scene with a retake, the destination, date and time, and comment of the retake are also shown. |
| **Scene Info Tags** | The scene info tags attached to the scene. |
| **History** | The log of the scene. |

In the flowchart view, the boxes of the workflows the scene is in are highlighted with a white frame, and the view moves so that they are visible.
Press the **✕** button at the top right to return to the scene list.

If a scene belongs to no workflow group and has no log, "The searched scene was not found." is shown.

## When you quit

If you try to close PPM before writing to Google Sheets or notifying Google Chat has finished, a warning is shown.
Press **Wait** to keep PPM open and continue sending.
Press **Quit anyway** to quit; the unsent data is not written or sent.

---

[← Back to contents](../index.md)
