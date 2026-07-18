### Task 5: Regenerate and Audit the DriverLib Configuration

**Files:**
- Generated: `Debug/ti_msp_dl_config.c`
- Generated: `Debug/ti_msp_dl_config.h`
- Generated if emitted: `Debug/Event.dot`
- Reference only: `NewProject1.syscfg:14-203`
- Reference only: `WIRING_AND_SYSCONFIG.md:1-117`

**Interfaces:**
- Consumes: `NewProject1.syscfg`, SDK 2.10.00.04 product metadata, SysConfig 1.26.2.4477.
- Produces: generated DriverLib symbols including `ICM42688_TIMER_INST == TIMG12` and unchanged documented hardware mappings.

- [ ] **Step 1: Capture the generated-file baseline without editing it**

Run:

```powershell
$baseline = "C:\Users\Joelin\AppData\Local\Temp\opencode\m0-sysconfig-baseline"
if (-not (Test-Path -LiteralPath "C:\Users\Joelin\AppData\Local\Temp\opencode")) {
    throw "Approved temp parent is missing"
}
if (-not (Test-Path -LiteralPath $baseline)) {
    New-Item -ItemType Directory -Path $baseline | Out-Null
}
Copy-Item -LiteralPath ".\Debug\ti_msp_dl_config.c" -Destination "$baseline\ti_msp_dl_config.c"
Copy-Item -LiteralPath ".\Debug\ti_msp_dl_config.h" -Destination "$baseline\ti_msp_dl_config.h"
if (Test-Path -LiteralPath ".\Debug\Event.dot") {
    Copy-Item -LiteralPath ".\Debug\Event.dot" -Destination "$baseline\Event.dot"
}
```

Expected: baseline copies exist only in the approved temp directory.

- [ ] **Step 2: Generate from the authoritative `.syscfg` file**

Run:

```powershell
& "C:\ti\sysconfig_1.26.2\sysconfig_cli.bat" `
  --product "..\..\controller\documents\sdk\.metadata\product.json" `
  --device MSPM0G3507 `
  --package "LQFP-48(PT)" `
  --script ".\NewProject1.syscfg" `
  --output ".\Debug" `
  --compiler keil
if ($LASTEXITCODE -ne 0) { throw "SysConfig generation failed" }
```

Expected: SysConfig exits `0` with no resource conflict.

- [ ] **Step 3: Verify the dedicated timer and fixed peripheral assignments**

Run:

```powershell
$header = Get-Content -Raw -LiteralPath ".\Debug\ti_msp_dl_config.h"
$required = @(
    '#define ICM42688_TIMER_INST\s+\(TIMG12\)',
    '#define CONTROL_TIMER_INST\s+\(TIMG0\)',
    '#define CONTROL_TIMER_INST_LOAD_VALUE\s+\(63999U\)',
    '#define MOTOR_PWM_A_INST\s+TIMA0',
    '#define MOTOR_PWM_B_INST\s+TIMA1',
    '#define I2C0_INST\s+I2C0',
    '#define SPI1_INST\s+SPI1',
    '#define DMA_SPI1_RX_CHAN_ID\s+\(2\)',
    '#define DMA_SPI1_TX_CHAN_ID\s+\(3\)',
    '#define ICM42688_CS_PIN\s+\(DL_GPIO_PIN_20\)',
    '#define MOTOR_DIR_M1_IN1_PIN\s+\(DL_GPIO_PIN_6\)',
    '#define MOTOR_DIR_M4_IN2_PIN\s+\(DL_GPIO_PIN_24\)',
    '#define ENCODER_ENC1_A_PIN\s+\(DL_GPIO_PIN_12\)',
    '#define ENCODER_ENC4_B_PIN\s+\(DL_GPIO_PIN_18\)'
)
foreach ($pattern in $required) {
    if ($header -notmatch $pattern) { throw "Generated invariant missing: $pattern" }
}
```

Expected: every invariant matches. If a generated macro uses a different stable prefix than shown for encoder pins, inspect the actual generated identifier and compare its port/pin/IOMUX value against `NewProject1.syscfg`; do not weaken or delete the hardware assertion.

- [ ] **Step 4: Audit all documented generated mappings**

Compare `Debug/ti_msp_dl_config.h` against `NewProject1.syscfg` and `WIRING_AND_SYSCONFIG.md` for:

```text
CONTROL_TIMER: TIMG0, 63999 load
ICM42688_TIMER: TIMG12, 1 MHz, maximum period, no interrupt
SPI1: PA16 POCI, PA18 PICO, PA17 SCLK, DMA channels 2/3
ICM42688 CS: PB20
I2C0: PA0 SDA, PA1 SCL, 100 kHz
UART0: PA10/PA11, UART1: PA8/PA9, UART3: PB2/PB3, all 115200
PWM: PA21/TIMA0 CCP0, PA22/TIMA0 CCP1, PA24/TIMA1 CCP1, PA25/TIMA0 CCP3
Motor direction: PB6, PB7, PB8, PA7, PA15, PB9, PB19, PB24
Encoders: PA12, PA13, PA2, PA26, PA27, PA28, PA31, PB18
```

Expected: every mapping is unchanged except the newly generated dedicated timer. If any mismatch exists, stop here and present the diff to the user.

- [ ] **Step 5: Review generated differences**

Run:

```powershell
git diff -- Debug/ti_msp_dl_config.c Debug/ti_msp_dl_config.h Debug/Event.dot
git diff --stat -- Debug/ti_msp_dl_config.c Debug/ti_msp_dl_config.h Debug/Event.dot
git diff --check -- Debug/ti_msp_dl_config.c Debug/ti_msp_dl_config.h Debug/Event.dot
```

Expected: differences are attributable to SysConfig 1.26.2 generation and the `ICM42688_TIMER` addition. `git diff --check` exits `0`. Stop for user review if the diff is unexpectedly broad or changes existing mappings.

- [ ] **Step 6: Re-run host tests after generation**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File ".\tests\run_tests.ps1"
```

Expected: ends with `Host tests: PASS`.

---

