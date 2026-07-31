### Task 8: STM32 Cross-Compilation Integration

**Files:**
- Modify: `CMakeLists.txt`
- Verify: all `App/Inc/*.h`, `App/Src/*.c`

**Interfaces:**
- Consumes: all protocol-independent core modules from Tasks 1-7.
- Produces: an ARM firmware ELF containing the control core but not invoking it from `main.c`.

- [ ] **Step 1: Establish the failing firmware link check**

Run:

```powershell
cmake --preset Debug
cmake --build --preset Debug
Test-Path -LiteralPath "build/Debug/CMakeFiles/balanceBall.dir/App/Src/balance_loop.c.obj"
```

Expected: firmware may build, then `Test-Path` prints `False` because the App sources are not compiled.

- [ ] **Step 2: Add the protocol-independent sources and includes to the target**

Replace the empty user sections in root `CMakeLists.txt` with:

```cmake
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    App/Src/balance_observer.c
    App/Src/balance_target.c
    App/Src/balance_controller.c
    App/Src/balance_actuator.c
    App/Src/balance_measurement.c
    App/Src/balance_supervisor.c
    App/Src/balance_loop.c
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    App/Inc
)
```

- [ ] **Step 3: Run host tests and a clean ARM build**

Run:

```powershell
cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure
cmake --preset Debug
cmake --build --preset Debug --clean-first
arm-none-eabi-nm build/Debug/CMakeFiles/balanceBall.dir/App/Src/balance_loop.c.obj | rg "balance_loop_init"
```

Expected:

- Host CTest: `100% tests passed, 0 tests failed`.
- ARM build exits `0` and produces `build/Debug/balanceBall.elf`.
- `nm` prints one `balance_loop_init` symbol from the ARM object file, proving the core was cross-compiled. The function may be absent from the final ELF because it is intentionally not called yet and linker garbage collection may remove it.

- [ ] **Step 4: Check final diff scope and generated-file safety**

Run:

```powershell
git diff --check
git status --short
git diff -- CMakeLists.txt App tests
```

Expected: no whitespace errors; intended changes are limited to `CMakeLists.txt`, `App/`, and `tests/`. Do not stage `build/`, `.superpowers/`, the photo, deleted PDFs, or unrelated `v1.0` work.

- [ ] **Step 5: Commit cross-compilation integration**

```powershell
git add CMakeLists.txt App tests
git commit -m "build: link balance control core"
```

## Completion Gate

Before claiming this plan implemented, run fresh:

```powershell
cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure
cmake --build --preset Debug --clean-first
git status --short
```

Report the exact CTest pass count, ARM build exit status, resulting ELF path, and any remaining unrelated worktree changes. Do not claim physical closed-loop operation: that requires the camera protocol, stepper protocol, hardware adapter plan, bench tests, and user acceptance.

## Required Follow-Up Inputs

The hardware-integration plan starts only after receiving both wire protocols. It must define exact frame bytes, checksum behavior, parser recovery, DMA framing method, driver units, safe stop command, status response, command refresh limit, and UART assignment. It will then cover `main.c` integration, UART callbacks, telemetry, manual-zero input, open-loop check commands, and hardware verification.
