[日本語](contributing.ja.md)

# Contributing

PPM is open-source software.
Contributions of any kind are welcome: fixes to the source code, translations, improvements to the manual, and so on.
Your contributions are released under the BSD 3-Clause License, as described in [License](license.md).

## How to contribute

The PPM repository is [opentoonz/ppm](https://github.com/opentoonz/ppm) on GitHub.

1. For large changes, discuss them in an [Issue](reporting-issues.md) first, so that your work is not wasted by a difference in direction.
2. Fork the repository, and make your changes on a working branch.
3. If you changed the source code, format it before committing (see "Coding style" below).
4. Open a pull request to the `main` branch.

## About pull requests

A pull request can be merged only when the reviewers can understand it.
As more people develop with AI, it has become easy to create pull requests that bundle many changes.
To keep reviews manageable, please help us with the following.

- **One pull request for one improvement.** If unrelated fixes are mixed in, we cannot merge only part of them.
- **If a change gets large, consider whether it can be split.** For example, put preparatory refactoring and the new feature in separate pull requests.
- **Write not only what you changed, but why it is needed and what the design intends.** For example, in what situation in production it helps, and why you chose this approach over others.
  If the code was written with AI, including the prompts you used also helps convey the intent.
- **If you changed or added a feature, also update the manual.** If you did not, say so in the pull request (see "Helping with the manual" below).

When you open a pull request, the description is filled in with a template for these points.
You do not need to get everything perfect. If you are unsure, feel free to ask in the pull request or an Issue.

If you develop with an AI coding tool, have it read `AGENTS.md` in the root of the repository. It summarizes the rules of this repository (the scope of changes, coding style, updating the manual, and so on). Claude Code reads it automatically through `CLAUDE.md`.

## Building from source

### Windows requirements

| Item | Details |
|---|---|
| OS | Windows 10 / 11 (64-bit) |
| Compiler | Visual Studio 2022 ("Desktop development with C++") |
| CMake | 3.5 or later |
| Qt | Qt 6 (MSVC 2022 64-bit). Qt 6.12 is used for development. When installing, also select the additional library **Qt 5 Compatibility Module**. |

The Qt modules used are Qt Widgets, Qt Network, Qt Core5Compat, Qt Concurrent, Qt QML, and Qt Linguist Tools.

### Building on Windows

Run the following in the repository folder.
For `QTDIR`, give the folder where Qt is installed (the folder that has `bin` and `lib`).
You can also set the environment variable `QTDIR` instead.

```bash
cmake -S sources -B build -G "Visual Studio 17 2022" -A x64 -DQTDIR=C:/Qt/6.12.0/msvc2022_64
```

```bash
cmake --build build --config Release
```

This creates `build/Release/ppm.exe`.
You can also open `ppm.sln` in the `build` folder in Visual Studio and build it there.

### Running on Windows

To run `ppm.exe`, you need the Qt DLLs and `conf.ini`.

1. Use Qt's `windeployqt` to copy the required DLLs next to `ppm.exe`.

   ```bash
   C:/Qt/6.12.0/msvc2022_64/bin/windeployqt.exe build/Release/ppm.exe
   ```

2. Create `conf.ini` next to `ppm.exe`, and set the `stuff` folder of the repository as the Stuff folder (see [Installation](../getting-started/installation.md)).

   ```ini
   [General]
   StuffFolderPath=../../stuff
   ```

The `stuff` folder has the sample project and data.
To check that your build works, first follow the steps of the [Tutorial: Moving a Scene Through the Pipeline](../getting-started/tutorial.md).

### Building and starting on macOS

Source builds target **macOS 15 or later on Apple Silicon (arm64), with Qt 6.12**. Intel Macs are outside the supported scope. Install Xcode 16 or later (including its command-line tools), CMake, and Qt for macOS with **Qt SVG** and the **Qt 5 Compatibility Module**. Use a CMake version supported by your Qt installation.

Run these commands from the repository folder, adjusting `QTDIR` to the Qt installation directory:

```bash
export QTDIR="$HOME/Qt/6.12.0/macos"
"$QTDIR/bin/qt-cmake" -S sources -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DQTDIR="$QTDIR"
cmake --build build --parallel
```

This creates `build/PPM.app`. The build uses Qt's default deployment target; PPM's supported macOS versions start at 15. PPM currently uses a light appearance even when macOS is in Dark Mode.

Create the per-user configuration directory:

```bash
mkdir -p "$HOME/Library/Application Support/PPM"
```

Create `~/Library/Application Support/PPM/conf.ini` and set the absolute path to your Stuff folder (replace the example path with your checkout location):

```ini
[General]
StuffFolderPath=/absolute/path/to/ppm/stuff
```

Start the application with `open build/PPM.app`. Keep the Qt installation available: this is a source build linked to the installed Qt libraries, not a self-contained distribution. Its generated `Contents/Resources/qt.conf` points to the plugins from that Qt installation. With a multi-configuration generator such as Xcode, build with `--config Release` and start `build/Release/PPM.app`. All builds use the same per-user configuration directory.

PPM resolves a relative `StuffFolderPath` from `~/Library/Application Support/PPM`, independently of the app location and working directory. An absolute local path such as `/Volumes/anime/PPM/stuff` also works. Per-user settings are stored in `~/Library/Application Support/PPM/usersettings.ini`; see [The Stuff Folder](../administration/stuff-folder.md) and [Preferences](../using/preferences.md).

The experimental macOS build supports external applications and the Zip Folder step. For Windows and macOS clients sharing project data, configure the Stuff folder and path mappings as described in [Running PPM on a Shared Folder](../administration/shared-operation.md). Use a separate test copy of the Stuff folder and production data when evaluating it. Opening the sample project can update its progress files.

### Focused behavior tests

Add `-DPPM_BUILD_TESTS=ON` to the configure command, build, and run:

```bash
ctest --test-dir build -C Release -R '^ppm_(configuration|sceneinfotag|confirmation|macos_operations)_tests$' --output-on-failure
```

GitHub Actions builds PPM with Qt 6.12 on Windows x64 and macOS arm64. Both platforms test configuration lookup, Stuff paths, per-user settings, scene-tag destruction, and confirming or canceling forward and rewind actions. The macOS tests also open documents in a test application, check OpenToonz argument delivery, and exercise ZIP creation and undo.

### Downloading CI application packages

After a successful **Build** workflow run, open that run in the repository's **Actions** tab and download its **Artifacts**. The artifact names include the platform and source commit. They are kept for 14 days. A GitHub login is required to download them.

Extract the downloaded artifact ZIP, then extract the application archive inside it:

- `ppm-windows-x64.zip`: contains `PPM/ppm.exe`, Qt runtime dependencies, and `conf.ini` pointing to the included `stuff` folder. The Visual C++ runtime DLLs are included next to `ppm.exe`.
- `ppm-macos-arm64.tar.gz`: contains `PPM/PPM.app` with Qt frameworks and plugins. The tar archive preserves executable permissions and framework symlinks. Configure `~/Library/Application Support/PPM/conf.ini` as described above, using the absolute path to the extracted `PPM/stuff` folder. The app has an ad-hoc signature and is not notarized. If macOS blocks a build you trust, allow it in **System Settings > Privacy & Security > Open Anyway**.

Both packages include sample data, license files, English and Japanese manuals, and `BUILD.txt` identifying the source commit. These are development builds for evaluation with copies of your data. The macOS feature limitations listed above also apply to these packages.

CI deploys Qt into a separate staging directory, checks macOS dependency paths and signatures, and checks that the packaged application stays running for five seconds without Qt on `PATH`. This startup smoke check terminates the process afterward; it does not exercise UI interactions or production workflows.

### Coding style

Before opening a pull request, always run clang-format on the C++ code you changed.
The formatting settings are in `sources/.clang-format`.

Run `beautification.sh` in the `sources` folder as follows (in Git Bash or a similar shell).

```bash
cd sources
./beautification.sh
```

This runs clang-format on all the `.cpp` and `.h` files changed from the `main` branch.
You can use the clang-format included in Visual Studio or the one from LLVM. Put it in a folder on your PATH.

Write the text shown on the screen in English, and wrap it in `tr()`. The Japanese translation goes into the translation file (see below).

## Helping with translations

You can add UI languages to PPM with translation files.
How to make a translation file is described in "Adding a language" in [UI Language and Translation Files](../administration/translations.md).

- If you want your translation included in PPM, send the translated `.ts` file (`sources/translations/PPM_<language code>.ts`) in a pull request. For a new language, also add the language code to `LANGUAGES` in `sources/CMakeLists.txt`.
- Reports of mistakes or unclear wording in the existing (Japanese) translation are also welcome.

## Helping with the manual

This manual is a set of Markdown files in the `docs` folder of the repository.

- Each page has a Japanese version (`*.ja.md`) and an English version (`*.md`). When you change one, change the other as well. If you can change only one, say so in the pull request, and we will update the other.
- Screenshots go in `docs/images/ja`. The English pages also use the Japanese screenshots.
- The manual describes how PPM behaves now. If you think something behaves wrongly, report it in an [Issue](reporting-issues.md) instead of changing the manual.

---

[← Back to contents](../index.md)
