# Emm V5 Task 2 Report: Response Parsing

## Scope

Implemented strict Emm V5 response parsing in the Task 2 file list:

- `App/Inc/emm_v5_protocol.h`
- `App/Src/emm_v5_protocol.c`
- `tests/test_emm_v5_protocol.c`

The legacy references in `temp/SM.c` and `temp/SM.h` were read only and were not modified.

## API Added

- `EmmV5Ack` defines complete (`0x02`), start (`0x12`), end (`0x22`), home-failed (`0x9F`), conflict (`0xE2`), and bad-command (`0xEE`) responses.
- `EmmV5Pid` exposes signed 32-bit `kp`, `ki`, and `kd` values.
- `emm_v5_parse_ack()` accepts the expected command function because ACK frames are shared by several command types.
- `emm_v5_parse_position()`, `emm_v5_parse_status()`, and `emm_v5_parse_pid()` bind to functions `0x36`, `0x3A`, and `0x21` respectively.

## Validation Behavior

Every parser rejects null input/output pointers with `EMM_V5_INVALID_ARGUMENT`. It checks exact frame length and the `0x6B` trailer before reading response fields, returning `EMM_V5_INVALID_FRAME` for structural failures. A valid frame with an unexpected address or function returns `EMM_V5_UNEXPECTED_RESPONSE`.

ACK values `0x02`, `0x12`, and `0x22` return `EMM_V5_OK`. ACK values `0x9F`, `0xE2`, and `0xEE` are decoded into the output value and return `EMM_V5_DRIVER_ERROR`. Unknown ACK status bytes are rejected as invalid frames.

Position frames interpret sign `0x00` as non-negative and `0x01` as negative. The parser rejects an invalid sign and magnitudes greater than `INT32_MAX`; this includes negative magnitude `0x80000000`, which cannot be represented as a positive `int32_t` before negation.

PID frames are exactly 15 bytes. Each big-endian 32-bit field is first assembled with unsigned shifts, then copied with `memcpy` into the signed output fields, avoiding implementation-defined signed shifts.

## TDD Evidence

### RED

Added parser tests before adding Task 2 declarations or implementation. The required focused build command was run:

```text
cmake --build build/host-tests --target test_emm_v5_protocol
```

It failed as expected at test compilation because `EmmV5Ack`, `EmmV5Pid`, ACK constants, and all four parser declarations did not yet exist. The compiler also reported implicit parser declarations. This proves the tests exercised the initially missing public API rather than pre-existing behavior.

### GREEN

Added the minimum parser types and strict implementations, then reran:

```text
cmake --build build/host-tests --target test_emm_v5_protocol
ctest --test-dir build/host-tests -R emm_v5_protocol --output-on-failure
```

Result: the target built with its existing `-Wall -Wextra -Werror` flags and `emm_v5_protocol` passed, 1/1 tests.

## Test Coverage

- All six defined ACK status values, including driver-error result mapping while retaining the decoded ACK.
- Positive and negative position conversion.
- Rejection of negative `0x80000000` position magnitude.
- Status byte decoding.
- Signed big-endian PID decoding and rejection of the 19-byte X-series PID response.
- Null input and output pointers for all parsers.
- Malformed lengths and invalid trailer.
- Unexpected address and function.

## Final Verification

Reconfigured the existing host test build without selecting a generator:

```text
cmake -S tests -B build/host-tests
```

Then ran the focused protocol suite and the full host suite:

```text
cmake --build build/host-tests --target test_emm_v5_protocol
ctest --test-dir build/host-tests -R emm_v5_protocol --output-on-failure
cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure
```

Results: focused `emm_v5_protocol` passed 1/1. Full host suite passed 2/2 (`balance_core`, `emm_v5_protocol`).

## Self-Review

No issues found in the Task 2 implementation. The only residual concern is hardware protocol confirmation: host tests verify documented frame layouts and result mapping, while UART integration and live motor responses remain outside Task 2.
