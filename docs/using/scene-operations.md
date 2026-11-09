[日本語](scene-operations.ja.md)

# Working on a Scene

This page explains the operations you can perform from each row of the scene list.

## Opening files

The **Open** column has the following buttons.

| Button | What it does |
|---|---|
| **Open** | Opens the file of the scene in the application for your work. |
| **Open XDTS** (XDTS icon) | Opens the timesheet (`.xdts`) of the scene. Disabled if no timesheet is found. |
| **Open Containing Folder** (folder icon) | Opens the folder that contains the cue file in Explorer on Windows or Finder on macOS. |
| **View Log** (log icon) | Shows the log of the scene in a separate window. Disabled if there is no log yet. |

### The Open button

What the **Open** button does depends on the type of the cue file.

- **`.clip`, `.tvpp`, or `.tnz` files**: Opened with the application set in [Preferences](preferences.md). An empty path uses the operating system's default application for that extension.
- **Other files** (such as marker text files): The scene folder is opened.
  If a "subfolder for the Open button" is set for the workflow group in the project settings, the work files in that subfolder are opened.
- **A shared scene with several cue files**: Pressing the button shows a menu, where you can choose the file to open.

The row of an opened file turns yellow. If the application cannot be started, a message is shown, and the color of the row does not change.

If several timesheets are found, you can also choose the file to open from a menu.
The application for opening timesheets is set in "XDTS application path" in [Preferences](preferences.md).

## Running an action

Press a button in the **Action** column to run the action.

1. A confirmation dialog appears.
2. Check the contents and press **OK**. The steps are run from top to bottom.
3. When "Action Completed." appears, the action is done.

![The action confirmation dialog](../images/ja/getting-started/tutorial-action-confirmation.png)
<!-- English screenshot to be made: images/en/getting-started/tutorial-action-confirmation.png -->

The top of the confirmation dialog shows the scene name, the workflow, and the action name.
Below it, the steps to be run are listed with the actual file names.
Check the following before pressing **OK**:

- Whether the file names of the rename and copy destinations are as intended
- Whether any step is marked with a warning icon (for example, a file is not found)

Some steps ask you to choose an option or enter something in the confirmation dialog.

When the action is run, the row turns gray, and an **Undo** button is shown in place of the action buttons.
Press **Refresh** to see in the list that the scene has moved to the next workflow.

For what to do if an action fails, see [How Scenes Flow Through Workflows](../concepts/scene-flow.md).

## Running actions together

A checkbox is shown at the left end of the rows of workflows that have only one action.
Turn on the checkboxes of several rows and then press the action button of any of those rows to run the actions of all the checked rows together.
For example, use this to approve many scenes at once in an episode director check.

![Rows with their checkboxes turned on for running actions together](../images/ja/using/multi-select.png)
<!-- English screenshot to be made: images/en/using/multi-select.png -->

- Checked rows turn blue.
- The confirmation dialog lists the steps of all the checked scenes.
- Rows in different workflows can be run together. Each row runs the action of its own workflow.
- If some scenes fail, " Some action(s) have failed." is shown. The rows of the scenes that succeeded turn gray, and the rows of the scenes that failed stay as they are.

If only one checkbox is on when you press an action button, only the action of that row is run, as usual.

## Workflow locks

You can lock a workflow with the lock icon at the right end of each workflow, in both the list view and the flowchart view.
In the rows of a locked workflow, the action buttons and the checkboxes for running actions together cannot be pressed.

At first, all workflows are locked.
If you unlock only the workflows you are in charge of, you can avoid pressing the actions of other workflows by mistake.
The lock state is recorded for each project. The list view and the flowchart view share the same state.

## Undo

Press the **Undo** button in a row that turned gray after running an action to revert the steps in reverse order.

1. A confirmation dialog for undoing appears ("[Undoing Action]" is shown at the top).
2. Press **OK**. The steps are reverted in reverse order, and the row returns to its normal color.

Undo has the following rules:

- The **Undo** button disappears when you press **Refresh**. If you want to undo, press it before pressing Refresh.
- Undoing is recorded in the log as "[UNDO]".
- Some steps, such as deleting files, cannot be undone. For such a step, "This action cannot be undone." is shown in the confirmation dialog.
- For an action that writes to Google Sheets, **Please Wait ...** is shown until the writing is done, and you cannot undo it. If the writing fails, **Undo (*)** is shown.

If you want to revert an action after pressing Refresh, fix the files by hand (see [Working with Files Directly](../concepts/working-with-files.md)).

## Rewind

In a workflow group set to use rewinds, a **Rewind** button (rewind arrow icon) is shown in the **Action** column of rows whose media is "Digital".
Use it to send a scene back to an earlier workflow in the same workflow group.
The mechanism is explained in [How Scenes Flow Through Workflows](../concepts/scene-flow.md).

1. Press the **Rewind** button.
2. "Before which workflow do you want to return ?" is shown. Choose the destination workflow and press **Select**.
   You can choose the current workflow or a workflow you can trace back to from it within the same workflow group.
3. Check the contents in the confirmation dialog and press **OK**.

![The dialog for choosing the destination of a rewind](../images/ja/using/rewind-destination.png)
<!-- English screenshot to be made: images/en/using/rewind-destination.png -->

When the rewind is run, the row turns gray and an **Undo** button is shown.
The log records "Rewind: Return to (workflow name)".

---

[← Back to contents](../index.md)
