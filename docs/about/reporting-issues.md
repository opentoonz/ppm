[日本語](reporting-issues.ja.md)

# Reporting Bugs and Requests

## Where to report

Bug reports and feature requests are accepted in GitHub [Issues](https://github.com/opentoonz/ppm/issues).

Before reporting, check the following.

- Whether [Troubleshooting and FAQ](../troubleshooting.md) already describes the same symptom and what to do
- Whether an Issue about the same thing already exists (if it does, add your information to that Issue)

Write only one bug or one request in each Issue.
You can write in English or Japanese.

## Information to include

When you report a bug, the following information helps find the cause.

| Information | How to find it |
|---|---|
| The version of PPM | Open **About PPM** from the **PPM** menu on macOS or the gear menu on Windows. The version and the build date are shown. |
| The version of Windows | Windows 10 or 11. |
| The language of PPM | **Language** in [Preferences](../using/preferences.md). |
| What you did | Write the buttons you pressed and the workflows and actions you chose, in order. |
| What happened, and what you expected to happen | Write both. |
| The messages shown | Write the warning text as it is. When an action fails, also include the contents of **Show Details...**. |
| The log of the scene | The lines of the log of the scenes involved (see "Tracing what happened from the log" in [Backup and Recovery](../administration/backup.md)). |
| Screenshots | They make it easier to see what was on the screen. |
| Whether it happens every time | Whether the same operation causes it every time, or only sometimes. |

If you can, check whether the same thing happens with the sample project in the distribution (the one used in the [Tutorial: Moving a Scene Through the Pipeline](../getting-started/tutorial.md)).
A bug that happens with the sample can be checked by the developers with the same steps.

## Information not to include

Anyone can read Issues.
Do not write or attach the following in an Issue. If needed, hide the names or replace them with made-up ones.

- Titles of productions, and images of unreleased productions
- Server names and paths of the shared folders
- Names of staff (such as the user names in the log)
- The Google Apps Script URL
- The project file (`.ppmprj`) itself. It contains information like the above.

## Security issues

Do not write security issues, such as obfuscated paths or the Google Apps Script URL being leaked, in the public Issues. Send them to the following email address instead.

- shun.iwasawa@ponoc.jp

Receiving a report does not mean a promise to work on solving the problem.
Developing and using open-source software is, as a rule, done at your own responsibility.

## Requests

Feature requests are also accepted in Issues.
It helps if you write the following.

- In what situation in production you have trouble
- How you deal with it now
- What you would like to happen

---

[← Back to contents](../index.md)
