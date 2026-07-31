# Emm V5 Task 1 Report

## Status

DONE

## Scope

Implemented the HAL-independent Emm V5 command encoder in the complete
`balanceBall` source workspace. Only the four Task 1 source/test files were
created or modified. This report is the explicitly exempt scratch artifact.

## Reference Review

Read `../temp/SM.c` and `../temp/SM.h` to derive command bytes. Neither file was
modified or added to a build.

SHA-256 before and after implementation:

- `SM.c`: `634421E0D84592FA447A77BD1C8AF8DD15CD7BC2794F49AD9C7CAD600E2D3F8F`
- `SM.h`: `B586B830C9A682DC6AC8DD811CA0C7DE4D8F5ACA1F4F2B2AD300C280311C92B1`

## RED Evidence

Tests and the dedicated CMake target were added before production code. The
following command used the existing configured generator:

```powershell
cmake -S tests -B build/host-tests
cmake --build build/host-tests --target test_emm_v5_protocol
```

CMake generation failed for the expected missing-feature reason:

```text
Cannot find source file:
  ../App/Src/emm_v5_protocol.c
No SOURCES given to target: test_emm_v5_protocol
```

## GREEN Evidence

After adding `emm_v5_protocol.h` and `emm_v5_protocol.c`:

```powershell
cmake -S tests -B build/host-tests
cmake --build build/host-tests --target test_emm_v5_protocol
ctest --test-dir build/host-tests -R emm_v5_protocol --output-on-failure
```

Result: the target compiled with `-Wall -Wextra -Werror`; focused CTest passed
`1/1` with no compiler warnings.

Full regression command:

```powershell
ctest --test-dir build/host-tests --output-on-failure
```

Result: `balance_core` and `emm_v5_protocol` passed, `2/2`, zero failures.

## Implemented API

The public header defines the required `EmmV5Result`, `EmmV5Direction`,
`EmmV5Frame`, and `EmmV5PositionCommand` shapes, plus a typed
`EmmV5HomeMode`. It exposes encoders for:

- enable `F3`
- velocity `F6`
- position `FD`
- fast-position setup `F1`
- fast move `FC`
- broadcast sync trigger `00 FF 66 6B`
- position query `36`
- status query `3A`
- microstep `84`
- stop `FE`
- set-zero `93`
- home `9A`
- abort-home `9C`
- PID query `21`

All addressed commands reject address `0x00`. The sync-trigger encoder has no
address argument and emits the fixed broadcast address. Byte fields retain
their full type ranges; direction and home-mode enums are semantically
validated. Binary fields use `bool`.

## Implementation Notes

One private bounded builder owns capacity validation, byte writes, frame-end
emission, and successful length publication. Explicit 16-bit and 32-bit
big-endian writers encode speed and pulse values. Every reported error leaves
the caller-visible frame length at zero, with no HAL dependency, global buffer,
delay, or mutable motor state.

## Test Coverage

The dedicated host test checks every command frame byte-for-byte. It also
checks a too-small output buffer, null frame/data/position-command pointers,
non-broadcast zero addresses, invalid direction values, invalid home mode, and
the required position command example.

## Self-Review

- Compared each opcode, magic byte, field order, sync flag, and frame length
  against `SM.c` and its protocol table in `SM.h`.
- Confirmed all multi-byte speed and pulse fields are big-endian.
- Confirmed output length is published only after the trailing `0x6B` is
  written.
- Confirmed source dependencies are limited to `stdbool.h`, `stddef.h`, and
  `stdint.h` through the public header.
- Confirmed no files outside Task 1 were changed by this implementation and
  unrelated worktree changes were preserved.

## Concerns

None for Task 1. Hardware behavior and response parsing are intentionally
outside this command-encoding task.
