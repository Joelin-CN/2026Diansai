# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.1.0] - 2026-07-30

### Fixed

- **Critical**: EKF observation matrix configuration error
  - Simplified 3-observation model to 2-observation model
  - Removed IMU gyro observation (observation[2]) to avoid low-cost IMU drift
  - Changed observation count from 3 to 2 (encoder velocity + encoder-derived omega only)
  - Reduced matrix inversion complexity from 3×3 to 2×2 (30% computation reduction)
  - Differentiated observation noise configuration (v: 0.03, ω: 0.08)
  - **Files**: `modules/Sens-Decision/inc/config.h`, `modules/Sens-Decision/src/EKF.c`, `modules/Sens-Decision/src/state_evaluate.c`, `modules/Sens-Decision/src/config.c`
  - **Analysis**: `build/logs/EKF_ANALYSIS_AND_FIX.txt`

- **Critical**: Stack overflow risk in FreeRTOS defaultTask
  - Moved 16 large EKF temporary matrices from stack to static storage (932 bytes)
  - Increased defaultTask stack size: 2048 → 3072 bytes (512 words → 768 words)
  - Added runtime stack watermark monitoring
  - Prevents HardFault during EKF matrix operations
  - **Files**: `modules/Sens-Decision/src/EKF.c`, `Core/Src/freertos.c`
  - **Safety**: Thread-safe (only one task calls EKF at 50Hz)

- **Major**: USART2 interrupt priority configuration conflict
  - Resolved dual priority configuration issue (HAL MSP vs module layer)
  - Documented USART2 Priority 3 design decision (high-speed 125Hz IR sensor data)
  - Added comprehensive ISR restrictions and safety guidelines
  - Clarified FreeRTOS API restrictions for Priority 0-4 interrupts
  - **Files**: `Core/Src/usart.c`, `Core/Src/stm32f4xx_it.c`, `modules/Sens-Decision/src/ir_uart_sensor.c`
  - **Documentation**: `docs/INTERRUPT_PRIORITY_GUIDE.md`

### Changed

- **Performance**: Optimized control frequency architecture
  - Decoupled encoder sampling (500Hz) from PID execution (100Hz)
  - Main loop frequency: 500Hz (unchanged)
  - Encoder sampling: 500Hz (unchanged)
  - PID control: 500Hz → 100Hz (80% computation reduction)
  - EKF/Perception: 50Hz (unchanged)
  - Rationale: Motor PWM response time ~10ms, so 100Hz PID is sufficient
  - **Files**: `modules/MotionControl/inc/motion_config.h`, `Core/Src/app/track_control_app.c`, `modules/MotionControl/src/motion_control.c`, `modules/MotionControl/src/motion_feedback.c`
  - **Documentation**: `docs/FREQUENCY_OPTIMIZATION_2026-07-30.txt`, `docs/FREQUENCY_ARCHITECTURE_DIAGRAM.txt`

### Added

- Comprehensive interrupt priority configuration guide
- EKF analysis and fix documentation
- Control frequency architecture documentation
- Stack usage monitoring in FreeRTOS task
- Frequency optimization analysis and verification plan
- Parameter configuration annotations and tuning guidelines

### Documentation

- Created `docs/INTERRUPT_PRIORITY_GUIDE.md` - FreeRTOS interrupt priority rules and safety guidelines
- Created `docs/FREQUENCY_OPTIMIZATION_2026-07-30.txt` - Control frequency optimization analysis
- Created `docs/FREQUENCY_ARCHITECTURE_DIAGRAM.txt` - Layered frequency architecture diagram
- Created `build/logs/EKF_ANALYSIS_AND_FIX.txt` - EKF observation model analysis
- Created `build/logs/2026-07-30_interrupt_priority_audit.md` - Interrupt priority audit report
- Updated `API_PITFALLS_GUIDE.md` - Added EKF and stack safety sections
- Updated `README.md` - Added recent modifications summary and updated API documentation

## [1.0.0] - 2026-07-29

### Initial Release

- Basic motor control with TB6612 driver
- Encoder feedback with 60000 PPR configuration
- IR sensor array integration (8-channel, UART protocol)
- IMU (ICM42688) integration with AHRS
- EKF state estimation (5-state: x, y, θ, v, ω)
- PID velocity control with feedforward compensation
- Square path and track path following
- FreeRTOS task scheduling (500Hz control loop)
- Sensor adapter layer with unified interface
- Perception layer (lateral error and heading error calculation)
- Behavior planner and trajectory generator

### Known Issues

- Full hardware validation pending
- Parameter tuning required for optimal performance
- Stack usage needs monitoring during runtime

---

## Version History Summary

| Version | Date | Key Changes |
|---------|------|-------------|
| 1.1.0 | 2026-07-30 | EKF fix, stack overflow prevention, frequency optimization, interrupt priority resolution |
| 1.0.0 | 2026-07-29 | Initial release with full control pipeline |

---

**Maintenance Notes:**

- All modifications in v1.1.0 completed by autonomous agent cluster (7 agents)
- Code changes validated through static analysis
- Hardware testing and validation pending
- See `docs/MODIFICATIONS_SUMMARY_2026-07-30.md` for detailed agent execution report

---

[Unreleased]: https://github.com/yourusername/project/compare/v1.1.0...HEAD
[1.1.0]: https://github.com/yourusername/project/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/yourusername/project/releases/tag/v1.0.0
