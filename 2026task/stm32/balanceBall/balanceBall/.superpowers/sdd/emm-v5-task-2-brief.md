### Task 2: Emm V5 Response Parsing

**Files:**
- Modify: `App/Inc/emm_v5_protocol.h`
- Modify: `App/Src/emm_v5_protocol.c`
- Modify: `tests/test_emm_v5_protocol.c`

**Interfaces:**
- Consumes: `EmmV5Result` and protocol constants from Task 1.
- Produces: `EmmV5Ack`, `EmmV5Pid`, `emm_v5_parse_ack()`, `emm_v5_parse_position()`, `emm_v5_parse_status()`, and `emm_v5_parse_pid()`.

- [ ] **Step 1: Add failing response-parser tests**

Add tests for ACK status `0x02`, `0x12`, `0x22`, `0x9F`, `0xE2`, and `0xEE`; signed position; status; and 15-byte Emm PID response. Include malformed length, wrong address, wrong function, and wrong `0x6B` trailer:

```c
static void test_position_response_preserves_sign(void)
{
    const uint8_t response[] = {0x01, 0x36, 0x01, 0x00, 0x00, 0x12, 0x34, 0x6B};
    int32_t position = 0;
    CHECK_TRUE(emm_v5_parse_position(0x01U, response, sizeof(response), &position)
               == EMM_V5_OK);
    CHECK_TRUE(position == -0x1234);
}

static void test_pid_response_rejects_x_series_length(void)
{
    uint8_t response[19] = {0x01, 0x21};
    EmmV5Pid pid;
    response[18] = 0x6B;
    CHECK_TRUE(emm_v5_parse_pid(0x01U, response, sizeof(response), &pid)
               == EMM_V5_INVALID_FRAME);
}
```

Define the sign mapping according to the Emm V5 response: sign byte `0x00` is non-negative and `0x01` is negative. Explicitly test negative magnitude `0x80000000`; reject it because it cannot be represented as a positive `int32_t` magnitude before negation.

- [ ] **Step 2: Run the parser tests and verify failure**

Run `cmake --build build/host-tests --target test_emm_v5_protocol`.

Expected: compilation fails for missing parser declarations or assertions fail for unimplemented parsing.

- [ ] **Step 3: Implement strict response parsing**

Add these public types:

```c
typedef enum {
    EMM_V5_ACK_COMPLETE = 0x02,
    EMM_V5_ACK_START = 0x12,
    EMM_V5_ACK_END = 0x22,
    EMM_V5_ACK_HOME_FAILED = 0x9F,
    EMM_V5_ACK_CONFLICT = 0xE2,
    EMM_V5_ACK_BAD_COMMAND = 0xEE
} EmmV5Ack;

typedef struct {
    int32_t kp;
    int32_t ki;
    int32_t kd;
} EmmV5Pid;
```

Every parser must validate pointer, exact length, expected address, expected function, and trailer before reading fields. ACK parser returns `EMM_V5_DRIVER_ERROR` for `E2`, `EE`, and `9F` while still exposing the decoded ACK; accepted progress codes are returned as `EMM_V5_OK`. Use unsigned shifts to assemble fields and `memcpy` into signed PID fields to avoid implementation-defined signed shifts.

- [ ] **Step 4: Run all protocol tests**

Run `cmake --build build/host-tests --target test_emm_v5_protocol; ctest --test-dir build/host-tests -R emm_v5_protocol --output-on-failure`.

Expected: all protocol tests pass.

- [ ] **Step 5: Commit response parsing**

```powershell
git add App/Inc/emm_v5_protocol.h App/Src/emm_v5_protocol.c tests/test_emm_v5_protocol.c
git commit -m "feat: parse Emm V5 motor responses"
```

