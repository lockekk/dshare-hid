# Notes for coding agents

## Running the unit tests on Windows

`build/` is the app build that `run_task.bat` configures. It has `BUILD_TESTS=OFF`, and options 2, 4
and 5 delete it. Build the tests in their own `build-tests/` folder, reusing the packages vcpkg
already installed in `build/`:

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build-tests -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_MANIFEST_MODE=ON -DVCPKG_MANIFEST_INSTALL=OFF ^
  -DVCPKG_INSTALLED_DIR=%CD%/build/vcpkg_installed ^
  -DVCPKG_TARGET_TRIPLET=x64-windows -DVCPKG_QT=ON ^
  -DBUILD_TESTS=ON -DSKIP_BUILD_TESTS=ON
cmake --build build-tests
ctest --test-dir build-tests/src/unittests --output-on-failure --timeout 60
```

- If there is no `build/vcpkg_installed` yet, leave out the two `VCPKG_..._INSTALL` / `_DIR` options
  and vcpkg installs into `build-tests/`.
- `build-tests/` is not in `.gitignore`. Put a `build-tests/.gitignore` containing `*` in it.
- If `vcvars64.bat` says `'vswhere.exe' is not recognized`, put
  `%ProgramFiles(x86)%\Microsoft Visual Studio\Installer` on `PATH` first. Run it through `cmd`, not
  Git Bash.
- `SKIP_BUILD_TESTS=ON` stops the build from running the tests itself, so you run `ctest` yourself
  and see its output.

### "No Qt platform plugin could be initialized"

On Windows, Qt looks for its platform plugin beside the `.exe`. Nothing deploys the plugin into the
test folders. A test with a `QApplication` then stops on a modal error dialog on the user's desktop,
and its process stays alive until someone closes the dialog. Under `ctest` it ends as
`Exit code 0xc0000409`.

`src/unittests/CMakeLists.txt` writes a `qt.conf` into every folder of tests (every `create_test`)
that points Qt at vcpkg's own plugins. If the dialog comes back:

1. Kill the stuck tests:
   `Get-Process | Where-Object { $_.Path -like '*\build-tests\*' } | Stop-Process -Force`
2. Check that the test's folder in `build-tests/src/unittests/` has a `qt.conf`. A test added
   without `create_test` doesn't get one.
3. Fix the CMake. Don't work around it with `QT_PLUGIN_PATH`, because the build's own `run_tests`
   step would still show the dialog.

### Running and reading the tests

- Run `ctest` serially, without `-j`. `DisplayLayoutStoreTests`, `DisplayLayoutViewTests`,
  `DisplayLayoutBondingTests` and `DisplayLayoutWriterTests` fail when they run in parallel and pass
  on their own. Their shared state hasn't been tracked down.
- On Windows the test programs print nothing to the console, so `--output-on-failure` shows nothing
  either. To see why a test failed, run it from its folder with Qt Test writing to a file:
  `.\ArchNetworkTests.exe -o "<scratch>\out.txt",txt`
- Running a GUI test outside `ctest`? Set `QT_QPA_PLATFORM=offscreen` so no windows open on the
  user's desktop.
