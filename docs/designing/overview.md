[日本語](overview.ja.md)

# Design Process and Preparation

This chapter explains how to build the workflows and file naming rules of a title as a PPM **project**.
It is written for pipeline designers (the people who decide the workflows of a title and create the PPM project).

Read the [How PPM Works](../concepts/glossary.md) chapter first, and make sure you know terms such as workflow, workflow group, cue file, and condition path.

## Design workflow

It is easiest to build a project in the following order.

1. **Write down how you work now**: List the workflows, the people in charge, and the rules for the names and locations of files (see "Writing down your current pipeline" below).
2. **[Basics](basics.md)**: Decide the project title, the scene number prefix, and the folders for the data.
3. **[Path Macros](path-macros.md)**: Register the location of the scene folder of each workflow group as a macro.
4. **[Workflow Groups](workflow-groups.md)**: Create groups of workflows such as LO, key animation, and in-between.
5. **[Scene Info Tags](info-tags.md)**: Prepare marks to attach to scenes, such as "Bank" (if needed).
6. **[Designing Workflows](workflows.md)**: Build the workflows with their condition paths, actions, and steps.
7. **[Saving and Testing a Project](saving-and-testing.md)**: Save the project and run a test scene through it.

You do not have to build everything from scratch.
The quickest way is to start from the sample project and rewrite it for your title (see "Creating a new project" below).

## Writing down your current pipeline

PPM copies the "handover by file names and locations" that your studio already does.
Before thinking of a new way of working, put your current way into a table.

| What to write down | Example (LO in the sample project) |
|---|---|
| Workflow groups | LO, key animation, in-between, ink and paint |
| Workflows and their order | Episode director check → director check → animation director check → assistant director check → LO up |
| The person in charge of each workflow | Episode director, director, animation director, assistant director |
| The file (name and location) while the scene is in each workflow | `c0001_LO_ej_s_k_e.clip` (episode director check), `c0001_LO_ej_s_k.clip` (director check), ... |
| What to do when passing to the next workflow | Rename the file, removing one mark from the end |
| What to do when passing to the next workflow group | Put a marker file in the key animation folder |

In the sample project, the marks of the people who have not checked the scene yet (`_e` episode director, `_k` director, `_s` animation director, `_ej` assistant director) are lined up at the end of the file name, and one is removed from the end each time a check is done.
Workflows do not have to be told apart by file names.
Some studios divide folders by the person in charge of each check, and hand scenes over by moving the whole scene folder.

```
[ROOT]/01_genga/kantoku/c0001/   … scenes for the director to check
[ROOT]/01_genga/sakkan/c0001/    … scenes for the animation director to check
```

In this case, the file names in the scene folder do not change; what changes in each workflow is the name of a folder in the middle of the path.
PPM condition paths are judged by the **whole path**, including the folders in the middle, not only by the file name, so this way of working can be copied as it is (e.g. `[ROOT]/01_genga/kantoku/[PREFIX][CUTNUMBER]/[PREFIX][CUTNUMBER]_genga.clip`).
The scene folder can be moved with the "Copy Folder Contents" step with **Transfer** set to "Move".

What matters is that you have a rule where **the path of a file (its name and location) tells which workflow the scene is in**.
If you have such a rule, you can use it as PPM condition paths as it is.

If your rules are not clear, decide them now.
Within one workflow group, make sure that the path of the cue file (the file name or the name of a folder in the middle) is different for every workflow.
If a file matches the condition paths of two workflows, the scene is listed in both.

## Deciding the folder structure

Also decide the structure of the folders for the scene files.
PPM expects the scene folders of each workflow group to be inside the **root folder** (`[ROOT]`) of the project.

The sample project has the following structure.

```
sample_data/                     … root folder [ROOT]
├─ 01_LO_genga/
│  └─ 00/                        … sequence [SEQUENCE]
│     └─ c0001/                  … prefix and scene number [PREFIX][CUTNUMBER]
│        ├─ LO/                  … scene folder for LO
│        └─ genga/               … scene folder for key animation
├─ 02_douga/
│  └─ 00/
│     └─ c0001/                  … scene folder for in-between
├─ 04_shiage/
│  └─ 00/
│     └─ c0001/                  … scene folder for ink and paint
└─ PPM/
   ├─ info/                      … project info folder
   └─ log/                       … log folder
```

Decide the following:

- The location of the scene folder of each workflow group (registered in [Path Macros](path-macros.md))
- Whether to divide scenes into folders of 100 scenes (sequences) ("Group Scenes by 100" in [Basics](basics.md))
- Whether to divide folders by episode (for a TV series, for example; "Has Episodes" in [Basics](basics.md))
- Where to create scene folders with take numbers (such as `c0001_t02`) for retakes (see [Workflow Groups](workflow-groups.md))
- The locations of the project info folder and the log folder

Put all these folders in a shared folder that everyone using PPM can read and write (see [Running PPM on a Shared Folder](../administration/shared-operation.md)).

## Opening the Project Settings dialog

Projects are designed in the **Project Settings** dialog.

1. At the top of the main window, choose the project you want to design.
2. On macOS, choose **Project → Project Settings → Basics** from the menu bar. On Windows, choose **Project Settings** from the gear menu.

![The Project Settings dialog](../images/ja/designing/project-settings-dialog.png)
<!-- English screenshot to be made: images/en/designing/project-settings-dialog.png -->

The left side of the dialog lists the setting categories.

| Category | Contents |
|---|---|
| **Basics** | Project title, prefix, folder locations, episodes, and so on ([Basics](basics.md)) |
| **Path Macros** | Macros used in paths ([Path Macros](path-macros.md)) |
| **Workflow Groups** | The list of workflow groups ([Workflow Groups](workflow-groups.md)) |
| **Info Tags** | Marks attached to scenes ([Scene Info Tags](info-tags.md)) |
| **Workflows - List** | Edit workflows in a list ([Designing Workflows](workflows.md)) |
| **Workflows - Flowchart** | Edit workflows in a flowchart ([Designing Workflows](workflows.md)) |

At the bottom left are the **project ID** field and the **Save** and **Close** buttons.
The project ID is the file name of the project (the `sample_project` part of `sample_project.ppmprj`), and it is the name shown in the **Project** field of the main window.
The title of the dialog shows "Project: project title (project ID.ppmprj)".

Changes are not written to the project file until you press **Save**.
While there are unsaved changes, `*` is shown at the end of the title.
Saving is explained in [Saving and Testing a Project](saving-and-testing.md).

### Creating a new project

To create a new project, save an existing project under another project ID.

1. Choose the project to start from (such as the sample project) and open **Project Settings**.
2. Enter a new project ID in the **Project ID** field at the bottom left (e.g. `my_title`).
3. Press **Save**.

The original project stays as it is, and a new project file with the same contents is created.
When you close the dialog, the main window switches to the new project.
Then rewrite the folder locations and other settings in **Basics** of the new project.

> **Note**: Until you change the folder locations, the new project looks at the same folders as the original project.
> Before running any action in the new project, change the root folder, project info folder, and log folder to the ones for the new project.

---

[← Back to contents](../index.md)
