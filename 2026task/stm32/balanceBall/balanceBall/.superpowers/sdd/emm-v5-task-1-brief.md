### Task 1: Emm V5 Command Encoding

**Files:**
- Create: `App/Inc/emm_v5_protocol.h`
- Create: `App/Src/emm_v5_protocol.c`
- Create: `tests/test_emm_v5_protocol.c`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: Only `<stdbool.h>`, `<stddef.h>`, and `<stdint.h>`.
- Produces: `EmmV5Result`, `EmmV5Frame`, `EmmV5PositionCommand`, and `emm_v5_encode_*()` functions used by Tasks 2-5.

- [ ] **Step 1: Add the failing command-frame tests**

Create a small test runner following `tests/test_balance.c`. Cover every command represented by the reference driver with byte-exact checks. The core expectations must include:

```c
static void test_position_command_is_big_endian(void)
{
    uint8_t bytes[EMM_V5_MAX_FRAME_SIZE] = {0};
    EmmV5Frame frame = { .data = bytes, .capacity = sizeof(bytes) };
    const EmmV5PositionCommand command = {
        .direction = EMM_V5_DIRECTION_CCW,
        .speed_rpm = 0x1234U,
        .acceleration = 0x56U,
        .pulse_count = 0x789ABCDEUL,
        .absolute = true,
        .synchronized = false,
    };

    CHECK_TRUE(emm_v5_encode_position(0x01U, &command, &frame) == EMM_V5_OK);
    CHECK_BYTES(frame.data,
                ((uint8_t[]){0x01, 0xFD, 0x01, 0x12, 0x34, 0x56,
                             0x78, 0x9A, 0xBC, 0xDE, 0x01, 0x00, 0x6B}),
                13U);
}

static void test_small_output_buffer_is_rejected(void)
{
    uint8_t bytes[4] = {0};
    EmmV5Frame frame = { .data = bytes, .capacity = sizeof(bytes) };
    CHECK_TRUE(emm_v5_encode_stop(0x01U, false, &frame) == EMM_V5_BUFFER_TOO_SMALL);
    CHECK_TRUE(frame.length == 0U);
}
```

Also assert exact frames for enable `F3`, velocity `F6`, fast-position setup `F1`, fast move `FC`, sync trigger `00 FF 66 6B`, position query `36`, status query `3A`, microstep `84`, stop `FE`, set-zero `93`, home `9A`, abort-home `9C`, and PID query `21`. Test null pointers, address `0x00` where broadcast is not explicitly allowed, and invalid direction/mode values.

Add a dedicated `test_emm_v5_protocol` executable to `tests/CMakeLists.txt` with `-Wall -Wextra -Werror` and CTest name `emm_v5_protocol`.

- [ ] **Step 2: Run the protocol test and verify the RED state**

Run:

```powershell
cmake -S tests -B build/host-tests -G Ninja
cmake --build build/host-tests --target test_emm_v5_protocol
```

Expected: compilation fails because `emm_v5_protocol.h` and its functions do not exist.

- [ ] **Step 3: Implement bounded command encoding**

Define the public shape exactly as follows, adding one encoder declaration per command listed above:

```c
#define EMM_V5_FRAME_END 0x6BU
#define EMM_V5_MAX_FRAME_SIZE 19U

typedef enum {
    EMM_V5_OK = 0,
    EMM_V5_INVALID_ARGUMENT,
    EMM_V5_BUFFER_TOO_SMALL,
    EMM_V5_INVALID_FRAME,
    EMM_V5_UNEXPECTED_RESPONSE,
    EMM_V5_DRIVER_ERROR
} EmmV5Result;

typedef enum {
    EMM_V5_DIRECTION_CW = 0,
    EMM_V5_DIRECTION_CCW = 1
} EmmV5Direction;

typedef struct {
    uint8_t *data;
    size_t capacity;
    size_t length;
} EmmV5Frame;

typedef struct {
    EmmV5Direction direction;
    uint16_t speed_rpm;
    uint8_t acceleration;
    uint32_t pulse_count;
    bool absolute;
    bool synchronized;
} EmmV5PositionCommand;
```

Use one private bounded builder and explicit big-endian writers. On any error set `frame->length = 0U`; never partially report a valid frame. Keep the exact frame formats from `../temp/SM.c`, except do not reproduce global buffers, HAL calls, delays, or mutable motor state.

- [ ] **Step 4: Run command encoding tests**

Run `cmake --build build/host-tests --target test_emm_v5_protocol; ctest --test-dir build/host-tests -R emm_v5_protocol --output-on-failure`.

Expected: `emm_v5_protocol` passes and compiler warnings are zero.

- [ ] **Step 5: Commit command encoding**

```powershell
git add App/Inc/emm_v5_protocol.h App/Src/emm_v5_protocol.c tests/test_emm_v5_protocol.c tests/CMakeLists.txt
git commit -m "feat: encode Emm V5 motor commands"
```

