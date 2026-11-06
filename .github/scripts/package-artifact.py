"""Stage, deploy, smoke-test, and archive the CI application (not the build tree)."""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile
import zipfile


def run(*args):
    subprocess.run([str(arg) for arg in args], check=True)


def deploy_msvc_runtime(build, package):
    # Use the VS instance selected by CMake and its newest x64 v143 CRT.
    cache = (build / "CMakeCache.txt").read_text(encoding="utf-8").splitlines()
    instance = next(line.split("=", 1)[1].split(",version=")[0]
                    for line in cache if line.startswith("CMAKE_GENERATOR_INSTANCE:"))
    redist = Path(instance) / "VC/Redist/MSVC"
    crt_subdir = Path("x64/Microsoft.VC143.CRT")
    versions = [path for path in redist.iterdir()
                if all(part.isdecimal() for part in path.name.split("."))
                and (path / crt_subdir).is_dir()]
    if not versions:
        raise RuntimeError(f"No redistributable x64 VC143 CRT in {redist}")
    latest = max(versions, key=lambda path: tuple(map(int, path.name.split("."))))
    crt = latest / crt_subdir
    # Match the five application-local runtime DLLs in the upstream win64 ZIP.
    for name in ("msvcp140.dll", "msvcp140_1.dll", "msvcp140_2.dll",
                 "vcruntime140.dll", "vcruntime140_1.dll"):
        shutil.copy2(crt / name, package / name)


def smoke_test(executable, qt_dir, temporary):
    """Check native startup with no project and no Qt installation on PATH."""
    env = os.environ.copy()
    for key in list(env):
        if key.startswith(("QT_", "QML", "DYLD_")):
            del env[key]
    env["PATH"] = os.pathsep.join(
        entry for entry in env.get("PATH", "").split(os.pathsep)
        if not Path(entry).is_relative_to(qt_dir)
    )
    home = temporary / "home"
    empty_stuff = temporary / "empty-stuff"
    home.mkdir()
    empty_stuff.mkdir()
    env["HOME"] = str(home)
    env["CFFIXED_USER_HOME"] = str(home)
    if sys.platform == "darwin":
        config_dir = home / "Library/Application Support/PPM"
        config_dir.mkdir(parents=True)
    else:
        config_dir = executable.parent
    (config_dir / "conf.ini").write_text(
        "[General]\nStuffFolderPath=" + empty_stuff.as_posix() + "\n",
        encoding="utf-8",
    )
    with tempfile.TemporaryFile() as output:
        process = subprocess.Popen(
            [str(executable)], cwd=temporary, env=env,
            stdout=output, stderr=subprocess.STDOUT,
        )
        try:
            try:
                code = process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                print("Native application stayed running for five seconds.", flush=True)
                if sys.platform == "win32":
                    module_paths = subprocess.check_output(
                        ["pwsh", "-NoProfile", "-Command",
                         f"(Get-Process -Id {process.pid} -ErrorAction Stop).Modules "
                         "| ForEach-Object { $_.FileName }"],
                        text=True,
                    ).splitlines()
                    for name in ("msvcp140.dll", "vcruntime140.dll"):
                        loaded = [Path(path) for path in module_paths
                                  if Path(path).name.lower() == name]
                        expected = executable.parent / name
                        if len(loaded) != 1 or not expected.samefile(loaded[0]):
                            raise RuntimeError(f"Expected {expected}, loaded {loaded}")
                        print(f"Verified application-local {name}: {loaded[0]}", flush=True)
            else:
                raise RuntimeError(f"Application exited during startup: {code}")
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
            output.seek(0)
            print(output.read().decode("utf-8", errors="replace"), flush=True)


def main():
    repository = Path(__file__).resolve().parents[2]
    build = repository / "build"
    qt_dir = Path(os.environ["QT_ROOT_DIR"]).resolve()
    if sys.platform not in ("darwin", "win32"):
        raise RuntimeError("CI application packages support Windows and macOS only")
    platform_name = "macos-arm64" if sys.platform == "darwin" else "windows-x64"
    with tempfile.TemporaryDirectory(prefix="ppm-artifact-") as temp:
        temporary = Path(temp)
        package = temporary / "PPM"
        package.mkdir()
        shutil.copytree(repository / "stuff", package / "stuff")
        shutil.copytree(repository / "docs", package / "docs")
        shutil.copy2(repository / "LICENSE", package / "LICENSE")
        shutil.copy2(repository / "package/README-ci.txt", package / "README.txt")
        if (qt_dir / "licenses").is_dir():
            shutil.copytree(qt_dir / "licenses", package / "stuff/LICENSE/Qt")

        if sys.platform == "darwin":
            bundle = package / "PPM.app"
            shutil.copytree(build / "PPM.app", bundle, symlinks=True)
            # The source build points to the runner's Qt plugins. Deployment
            # must instead use the plugins inside this fresh staging bundle.
            config = bundle / "Contents/Resources/qt.conf"
            config.unlink()
            run(qt_dir / "bin/macdeployqt", bundle, "-always-overwrite", "-no-codesign")
            config.write_text("[Paths]\nPlugins=PlugIns\n", encoding="utf-8")
            # Plugins can retain rpaths for the Qt installation layout. Give
            # each loader its own path to the deployed frameworks instead of
            # relying on the main executable's search paths.
            for plugin in (bundle / "Contents/PlugIns").rglob("*.dylib"):
                rpath = "@loader_path/" + os.path.relpath(
                    bundle / "Contents/Frameworks", plugin.parent)
                commands = subprocess.check_output(["otool", "-l", str(plugin)], text=True)
                if f"path {rpath} (offset" not in commands:
                    run("install_name_tool", "-add_rpath", rpath, plugin)
            run("cmake", f"-DBUNDLE={bundle}", "-P",
                repository / ".github/scripts/verify-macos-bundle.cmake")
            run("codesign", "--force", "--deep", "--sign", "-", bundle)
            run("codesign", "--verify", "--deep", "--strict", bundle)
            executable = bundle / "Contents/MacOS/PPM"
        else:
            executable = package / "ppm.exe"
            shutil.copy2(build / "Release/ppm.exe", executable)
            run(qt_dir / "bin/windeployqt.exe", "--release", "--no-compiler-runtime",
                executable)
            deploy_msvc_runtime(build, package)

        smoke_test(executable, qt_dir, temporary)
        if sys.platform == "win32":
            (package / "conf.ini").write_text(
                "[General]\nStuffFolderPath=stuff\n", encoding="utf-8")
        (package / "BUILD.txt").write_text(
            f"Commit: {os.environ['GITHUB_SHA']}\nPlatform: {platform_name}\nQt: 6.12.0\n",
            encoding="utf-8",
        )
        output = build / "artifacts"
        output.mkdir(exist_ok=True)
        if sys.platform == "darwin":
            # Preserve executable permissions and framework symlinks through
            # GitHub's outer artifact ZIP, which does not retain permissions.
            archive = output / f"ppm-{platform_name}.tar.gz"
            with tarfile.open(archive, "w:gz") as handle:
                handle.add(package, arcname="PPM")
        else:
            archive = output / f"ppm-{platform_name}.zip"
            with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as handle:
                for path in sorted(package.rglob("*")):
                    if path.is_file():
                        handle.write(path, path.relative_to(temporary))
        print(f"Packaged {archive}", flush=True)


if __name__ == "__main__":
    main()
