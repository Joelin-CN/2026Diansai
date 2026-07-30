/**
 * @file      track_path.c
 * @brief     400m track-shaped path implementation
 * @author    Claude (Kiro)
 * @version   1.0.0
 * @date      2026-07-30
 * @note      Track layout (top view, code frame: X=forward, Y=left):
 *
 *            Semicircle 2 (left)
 *                 ╭─────╮
 *           C ────╯     ╰──── D
 *                Straight 2
 *           │                 │
 *     Straight 1          Straight 1
 *           │                 │
 *           A ────╮     ╭──── B
 *                 ╰─────╯
 *            Semicircle 1 (right)
 *
 *  Geometry:
 *    - Semicircle radius R = 0.5m
 *    - Straight length L = 1.5m
 *    - Total perimeter = 2πR + 2L = π + 3.0 ≈ 6.14m
 *    - Point spacing ≤ 20mm → ~307 points
 */

#include <math.h>
#include "track_path.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Track geometry parameters */
#define TRACK_RADIUS 0.5f              /* Semicircle radius (m) */
#define TRACK_STRAIGHT_LENGTH 1.5f     /* Straight section length (m) */
#define TRACK_DIAMETER (2.0f * TRACK_RADIUS)  /* Distance between straights */

/* Path discretization - use fixed value to avoid VLA warning */
#define POINTS_PER_SEMICIRCLE 79       /* Fixed: ~1.57m / 0.02m */
#define POINTS_PER_STRAIGHT 75         /* Fixed: 1.5m / 0.02m */
#define TRACK_POINT_COUNT (2 * POINTS_PER_SEMICIRCLE + 2 * POINTS_PER_STRAIGHT + 1)  /* 309 points */
#define POINT_SPACING 0.02f            /* Target spacing: 20mm */
#define SEMICIRCLE_ARC_LENGTH (M_PI * TRACK_RADIUS)  /* π*R ≈ 1.57m */
#define STRAIGHT_LENGTH TRACK_STRAIGHT_LENGTH        /* 1.5m */

static path_point_t g_track_path[TRACK_POINT_COUNT];
static bool g_path_initialized = false;

/**
 * @brief Initialize track path with CCW traversal
 *
 * Coordinate frame (code frame):
 *   X = forward (vehicle forward direction)
 *   Y = left (vehicle left direction)
 *
 * Path starts at A (0, -R) and follows:
 *   A → Semicircle1 (CCW around bottom) → B → Straight2 (upward) →
 *   C → Semicircle2 (CCW around top) → D → Straight1 (downward) → A
 *
 * Key points:
 *   A: (0, -R) = (0, -0.5)        - Start of semicircle 1
 *   B: (L, -R) = (1.5, -0.5)      - End of semicircle 1, start of straight 2
 *   C: (L, +R) = (1.5, +0.5)      - End of straight 2, start of semicircle 2
 *   D: (0, +R) = (0, +0.5)        - End of semicircle 2, start of straight 1
 */
static void init_track_path(void) {
    size_t idx = 0;

    /* ===== Segment 1: Semicircle 1 (bottom/right, A → B) ===== */
    /* Center at (L/2, -R), angle from π (left) to 0 (right) */
    /* Parametric: x = L/2 + R*cos(θ), y = -R + R*sin(θ), θ ∈ [π, 2π] */
    float center1_x = TRACK_STRAIGHT_LENGTH / 2.0f;  /* 0.75m */
    float center1_y = -TRACK_RADIUS;                  /* -0.5m */

    for (size_t i = 0; i < POINTS_PER_SEMICIRCLE; ++i) {
        float t = (float)i / (float)POINTS_PER_SEMICIRCLE;
        float theta = (float)(M_PI + t * M_PI);  /* π → 2π (CCW from left to right) */

        g_track_path[idx].x = center1_x + TRACK_RADIUS * cosf(theta);
        g_track_path[idx].y = center1_y + TRACK_RADIUS * sinf(theta);
        g_track_path[idx].heading = theta + (float)(M_PI / 2.0);  /* Tangent direction */
        g_track_path[idx].curvature = 1.0f / TRACK_RADIUS;  /* κ = 1/R = 2.0 m⁻¹ */
        idx++;
    }

    /* ===== Segment 2: Straight 2 (right side, B → C) ===== */
    /* From (L, -R) to (L, +R), heading = +π/2 (upward/left in code frame) */
    for (size_t i = 0; i < POINTS_PER_STRAIGHT; ++i) {
        float t = (float)i / (float)POINTS_PER_STRAIGHT;

        g_track_path[idx].x = TRACK_STRAIGHT_LENGTH;
        g_track_path[idx].y = -TRACK_RADIUS + t * TRACK_DIAMETER;
        g_track_path[idx].heading = (float)(M_PI / 2.0);  /* +π/2 = left */
        g_track_path[idx].curvature = 0.0f;
        idx++;
    }

    /* ===== Segment 3: Semicircle 2 (top/left, C → D) ===== */
    /* Center at (L/2, +R), angle from 0 (right) to π (left) */
    /* Parametric: x = L/2 + R*cos(θ), y = +R + R*sin(θ), θ ∈ [0, π] */
    float center2_x = TRACK_STRAIGHT_LENGTH / 2.0f;  /* 0.75m */
    float center2_y = TRACK_RADIUS;                   /* +0.5m */

    for (size_t i = 0; i < POINTS_PER_SEMICIRCLE; ++i) {
        float t = (float)i / (float)POINTS_PER_SEMICIRCLE;
        float theta = t * (float)M_PI;  /* 0 → π (CCW from right to left) */

        g_track_path[idx].x = center2_x + TRACK_RADIUS * cosf(theta);
        g_track_path[idx].y = center2_y + TRACK_RADIUS * sinf(theta);
        g_track_path[idx].heading = theta + (float)(M_PI / 2.0);  /* Tangent direction */
        g_track_path[idx].curvature = 1.0f / TRACK_RADIUS;  /* κ = 1/R = 2.0 m⁻¹ */
        idx++;
    }

    /* ===== Segment 4: Straight 1 (left side, D → A) ===== */
    /* From (0, +R) to (0, -R), heading = -π/2 (downward/right in code frame) */
    for (size_t i = 0; i <= POINTS_PER_STRAIGHT; ++i) {  /* Include endpoint to close loop */
        float t = (float)i / (float)POINTS_PER_STRAIGHT;

        g_track_path[idx].x = 0.0f;
        g_track_path[idx].y = TRACK_RADIUS - t * TRACK_DIAMETER;
        g_track_path[idx].heading = (float)(-M_PI / 2.0);  /* -π/2 = right */
        g_track_path[idx].curvature = 0.0f;
        idx++;

        if (idx >= TRACK_POINT_COUNT) break;
    }

    g_path_initialized = true;
}

const path_point_t *TrackPath_GetPoints(void) {
    if (!g_path_initialized) {
        init_track_path();
    }
    return g_track_path;
}

size_t TrackPath_GetPointCount(void) {
    if (!g_path_initialized) {
        init_track_path();
    }
    return TRACK_POINT_COUNT;
}

float TrackPath_CorrectOmega(float nominal_omega, float lateral_error,
                             float heading_error,
                             const track_path_config_t *config) {
    if (config == NULL) {
        return nominal_omega;
    }

    /* Lateral correction: positive error (left of line) → negative omega (turn right) */
    float lateral_correction = -lateral_error * config->lateral_gain;

    /* Heading correction: positive error (pointing right) → negative omega (turn left) */
    float heading_correction = -heading_error * config->heading_gain;

    float corrected = nominal_omega + lateral_correction + heading_correction;

    /* Clamp to [-max_omega, +max_omega] */
    if (corrected > config->max_omega_radps) {
        corrected = config->max_omega_radps;
    } else if (corrected < -config->max_omega_radps) {
        corrected = -config->max_omega_radps;
    }

    return corrected;
}

bool TrackPath_UpdateLap(track_lap_counter_t *counter, size_t nearest_index,
                         size_t path_count, uint8_t target_laps) {
    if (counter == NULL || path_count == 0) {
        return false;
    }

    /* Validate target_laps: must be in range 1-10 */
    if (target_laps < 1 || target_laps > 10) {
        return false;
    }

    /* Guard zone is first 5% of path */
    size_t guard_threshold = path_count / 20;
    if (guard_threshold < 5) {
        guard_threshold = 5;
    }

    bool in_start_zone = (nearest_index < guard_threshold);

    /* If not in start zone, mark as having left */
    if (!in_start_zone) {
        counter->left_start_guard = true;
        return false;
    }

    /* In start zone: only increment if we've left before */
    if (counter->left_start_guard) {
        counter->completed_laps++;
        counter->left_start_guard = false;  /* Reset guard */

        /* Check if target reached */
        if (counter->completed_laps >= target_laps) {
            counter->target_reached = true;
        }

        return true;
    }

    return false;
}
