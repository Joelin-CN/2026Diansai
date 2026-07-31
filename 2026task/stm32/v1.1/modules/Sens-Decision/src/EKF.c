/**
 * @file      EKF.c
 * @brief     Extended Kalman Filter implementation
 * @author    joelin-CN
 * @version   1.0.0
 * @date      2026-07-17
 *
 * @note Stack Optimization (2026-07-30):
 *       Large temporary matrices moved to static storage to prevent stack overflow.
 *       This is safe because only one task calls EKF functions (defaultTask at 50Hz).
 *       Total stack savings: ~1284 bytes.
 */

#include "../inc/EKF.h"
#include "../inc/utils.h"
#include <math.h>
#include <string.h>

/* ============================================================================
 * Static Working Buffers (Stack Optimization)
 * ============================================================================ */

/**
 * @brief Static buffers for matrix operations to avoid stack overflow
 * @note These are shared across ekf_predict/ekf_update calls
 * @note Thread-safe: Only one task (defaultTask) calls EKF at 50Hz
 * @warning Do NOT call EKF functions from multiple threads/ISRs
 */
static float s_temp_5x5[5][5];       /* Shared by matrix ops: 100 bytes */
static float s_F[5][5];              /* ekf_predict: 100 bytes */
static float s_F_T[5][5];            /* ekf_predict: 100 bytes */
static float s_P_temp[5][5];         /* ekf_predict: 100 bytes */

static float s_H[2][5];              /* ekf_update: 40 bytes */
static float s_H_T[5][2];            /* ekf_update: 40 bytes */
static float s_S[2][2];              /* ekf_update: 16 bytes */
static float s_S_inv[2][2];          /* ekf_update: 16 bytes */
static float s_P_H_T[5][2];          /* ekf_update: 40 bytes */
static float s_K[5][2];              /* ekf_update: 40 bytes */
static float s_I_KH[5][5];           /* ekf_update: 100 bytes */
static float s_I_KH_T[5][5];         /* ekf_update: 100 bytes */
static float s_I_KH_P[5][5];         /* ekf_update: 100 bytes */
static float s_I_KH_P_I_KH_T[5][5];  /* ekf_update: 100 bytes */
static float s_K_R[5][2];            /* ekf_update: 40 bytes */
static float s_K_R_K_T[5][5];        /* ekf_update: 100 bytes */

/* Total static memory: 932 bytes */

static void matrix_multiply_5x5(float A[5][5], float B[5][5], float result[5][5]) {
    size_t i, j, k;

    for (i = 0; i < 5; ++i) {
        for (j = 0; j < 5; ++j) {
            s_temp_5x5[i][j] = 0.0f;
            for (k = 0; k < 5; ++k) {
                s_temp_5x5[i][j] += A[i][k] * B[k][j];
            }
        }
    }

    memcpy(result, s_temp_5x5, sizeof(s_temp_5x5));
}

static void matrix_transpose_5x5(float A[5][5], float result[5][5]) {
    size_t i, j;

    for (i = 0; i < 5; ++i) {
        for (j = 0; j < 5; ++j) {
            s_temp_5x5[i][j] = A[j][i];
        }
    }

    memcpy(result, s_temp_5x5, sizeof(s_temp_5x5));
}

static void symmetrize_5x5(float matrix[5][5]) {
    size_t i, j;
    
    for (i = 0; i < 5; ++i) {
        for (j = i + 1; j < 5; ++j) {
            float avg = (matrix[i][j] + matrix[j][i]) / 2.0f;
            matrix[i][j] = avg;
            matrix[j][i] = avg;
        }
    }
}

/**
 * @brief Initialize Extended Kalman Filter
 *
 * Sets up the EKF with initial covariance, process noise, and observation noise.
 *
 * @param ekf Pointer to EKF structure to initialize
 * @param config Pointer to configuration parameters
 *
 * @note State vector: [x, y, theta, v, omega]
 * @note Observation vector (2026-07-30): [v_encoder, omega_encoder]
 * @note Modified 2026-07-30: Changed from 3-observation to 2-observation model
 */
void ekf_init(ekf_t *ekf, const sd_ekf_config_t *config) {
    size_t i, j;
    
    memset(ekf, 0, sizeof(*ekf));
    
    for (i = 0; i < SD_EKF_STATE_COUNT; ++i) {
        ekf->covariance[i][i] = config->initial_covariance_diag[i];
        ekf->process_noise[i][i] = config->process_noise_diag[i];
    }
    
    for (i = 0; i < SD_EKF_OBSERVATION_COUNT; ++i) {
        ekf->observation_noise[i][i] = config->observation_noise_diag[i];
    }
    
    for (i = 0; i < SD_EKF_STATE_COUNT; ++i) {
        for (j = 0; j < SD_EKF_STATE_COUNT; ++j) {
            if (!isfinite(ekf->covariance[i][j]) || 
                !isfinite(ekf->process_noise[i][j])) {
                ekf->covariance[i][j] = 0.0f;
                ekf->process_noise[i][j] = 0.0f;
            }
        }
    }
}

/**
 * @brief Predict EKF state using motion model
 *
 * Implements constant velocity model with angular velocity:
 *   x' = x + v*cos(θ)*dt
 *   y' = y + v*sin(θ)*dt
 *   θ' = θ + ω*dt
 *   v' = v (constant)
 *   ω' = ω (constant)
 *
 * Updates state covariance: P' = F*P*F^T + Q
 *
 * @param ekf Pointer to EKF structure
 * @param dt Time step (seconds)
 *
 * @note Uses static buffers for matrix operations (not thread-safe)
 * @note Modified 2026-07-30: Moved matrices to static storage for stack safety
 *
 * @warning Do NOT call from multiple threads without mutex
 */
void ekf_predict(ekf_t *ekf, float dt) {
    float x = ekf->state[0];
    float y = ekf->state[1];
    float theta = ekf->state[2];
    float v = ekf->state[3];
    float omega = ekf->state[4];
    float cos_theta = cosf(theta);
    float sin_theta = sinf(theta);
    size_t i, j;

    ekf->state[0] = x + v * cos_theta * dt;
    ekf->state[1] = y + v * sin_theta * dt;
    ekf->state[2] = sd_normalize_angle(theta + omega * dt);
    ekf->state[3] = v;
    ekf->state[4] = omega;

    memset(s_F, 0, sizeof(s_F));
    for (i = 0; i < 5; ++i) {
        s_F[i][i] = 1.0f;
    }
    s_F[0][2] = -v * sin_theta * dt;
    s_F[0][3] = cos_theta * dt;
    s_F[1][2] = v * cos_theta * dt;
    s_F[1][3] = sin_theta * dt;
    s_F[2][4] = dt;

    matrix_transpose_5x5(s_F, s_F_T);
    matrix_multiply_5x5(ekf->covariance, s_F_T, s_P_temp);
    matrix_multiply_5x5(s_F, s_P_temp, ekf->covariance);

    for (i = 0; i < SD_EKF_STATE_COUNT; ++i) {
        for (j = 0; j < SD_EKF_STATE_COUNT; ++j) {
            ekf->covariance[i][j] += ekf->process_noise[i][j];
        }
    }
}

/**
 * @brief Update EKF state with measurement
 *
 * Implements Kalman filter update (correction) step:
 * 1. Compute innovation: y = z - H*x
 * 2. Compute innovation covariance: S = H*P*H^T + R
 * 3. Compute Kalman gain: K = P*H^T*S^(-1)
 * 4. Update state: x' = x + K*y
 * 5. Update covariance (Joseph form): P' = (I-K*H)*P*(I-K*H)^T + K*R*K^T
 *
 * @param ekf Pointer to EKF structure
 * @param observation Measurement vector [v_encoder, omega_encoder]
 *
 * @return SD_OK on success, SD_ERR_NUMERIC if matrix inversion fails
 *
 * @note Observation model (2026-07-30):
 *       - observation[0]: Linear velocity from encoders
 *       - observation[1]: Angular velocity from encoder differential
 *       - IMU gyro observation removed to avoid drift
 *
 * @note Uses 2×2 matrix inversion (analytical solution, fast and stable)
 * @note Modified 2026-07-30: Simplified from 3-observation to 2-observation model
 *
 * @warning Do NOT call from multiple threads without mutex
 */
sd_status_t ekf_update(ekf_t *ekf, const float observation[SD_EKF_OBSERVATION_COUNT]) {
    float y[2];
    float det;
    size_t i, j, k;

    /* Observation model H (2x5 matrix):
     * observation[0] = state[3]  (linear velocity from encoders)
     * observation[1] = state[4]  (angular velocity from encoder differential)
     *
     * NOTE: observation[2] (IMU gyro omega) is now IGNORED to simplify the model.
     * Reason: For line-following robots on flat surfaces, encoder-based omega
     * estimation is sufficiently accurate. Fusing with low-cost IMU gyro often
     * introduces drift and degrades performance rather than improving it.
     *
     * If high-quality IMU is available and slip is a concern, consider re-enabling
     * 3-observation fusion with properly tuned observation noise covariance.
     */
    memset(s_H, 0, sizeof(s_H));
    s_H[0][3] = 1.0f;  // Observe v (linear velocity)
    s_H[1][4] = 1.0f;  // Observe ω (angular velocity from encoders)

    /* Innovation (measurement residual) */
    y[0] = observation[0] - ekf->state[3];  // v error
    y[1] = observation[1] - ekf->state[4];  // ω error (encoder-based)

    /* Transpose H: H_T = H^T (5x2) */
    for (i = 0; i < 2; ++i) {
        for (j = 0; j < 5; ++j) {
            s_H_T[j][i] = s_H[i][j];
        }
    }

    /* Compute P*H^T (5x2) */
    for (i = 0; i < 5; ++i) {
        for (j = 0; j < 2; ++j) {
            s_P_H_T[i][j] = 0.0f;
            for (k = 0; k < 5; ++k) {
                s_P_H_T[i][j] += ekf->covariance[i][k] * s_H_T[k][j];
            }
        }
    }

    /* Innovation covariance: S = H*P*H^T + R (2x2) */
    for (i = 0; i < 2; ++i) {
        for (j = 0; j < 2; ++j) {
            s_S[i][j] = ekf->observation_noise[i][j];
            for (k = 0; k < 5; ++k) {
                s_S[i][j] += s_H[i][k] * s_P_H_T[k][j];
            }
        }
    }

    /* Compute determinant of 2x2 matrix S */
    det = s_S[0][0] * s_S[1][1] - s_S[0][1] * s_S[1][0];

    if (!isfinite(det) || fabsf(det) < 1e-10f) {
        return SD_ERR_NUMERIC;
    }

    /* Inverse of 2x2 matrix: S_inv = S^(-1) */
    s_S_inv[0][0] = s_S[1][1] / det;
    s_S_inv[0][1] = -s_S[0][1] / det;
    s_S_inv[1][0] = -s_S[1][0] / det;
    s_S_inv[1][1] = s_S[0][0] / det;

    /* Kalman gain: K = P*H^T*S^(-1) (5x2) */
    for (i = 0; i < 5; ++i) {
        for (j = 0; j < 2; ++j) {
            s_K[i][j] = 0.0f;
            for (k = 0; k < 2; ++k) {
                s_K[i][j] += s_P_H_T[i][k] * s_S_inv[k][j];
            }
        }
    }

    /* State update: x = x + K*y */
    for (i = 0; i < 5; ++i) {
        float correction = 0.0f;
        for (j = 0; j < 2; ++j) {
            correction += s_K[i][j] * y[j];
        }
        ekf->state[i] += correction;
    }

    /* Normalize angle to [-π, π] */
    ekf->state[2] = sd_normalize_angle(ekf->state[2]);

    /* Covariance update using Joseph form: P = (I-K*H)*P*(I-K*H)^T + K*R*K^T */
    memset(s_I_KH, 0, sizeof(s_I_KH));
    for (i = 0; i < 5; ++i) {
        s_I_KH[i][i] = 1.0f;
        for (j = 0; j < 5; ++j) {
            for (k = 0; k < 2; ++k) {
                s_I_KH[i][j] -= s_K[i][k] * s_H[k][j];
            }
        }
    }

    matrix_transpose_5x5(s_I_KH, s_I_KH_T);
    matrix_multiply_5x5(s_I_KH, ekf->covariance, s_I_KH_P);
    matrix_multiply_5x5(s_I_KH_P, s_I_KH_T, s_I_KH_P_I_KH_T);

    /* Compute K*R*K^T */
    for (i = 0; i < 5; ++i) {
        for (j = 0; j < 2; ++j) {
            s_K_R[i][j] = 0.0f;
            for (k = 0; k < 2; ++k) {
                s_K_R[i][j] += s_K[i][k] * ekf->observation_noise[k][j];
            }
        }
    }

    for (i = 0; i < 5; ++i) {
        for (j = 0; j < 5; ++j) {
            s_K_R_K_T[i][j] = 0.0f;
            for (k = 0; k < 2; ++k) {
                s_K_R_K_T[i][j] += s_K_R[i][k] * s_K[j][k];
            }
        }
    }

    /* Final covariance: P = (I-K*H)*P*(I-K*H)^T + K*R*K^T */
    for (i = 0; i < 5; ++i) {
        for (j = 0; j < 5; ++j) {
            ekf->covariance[i][j] = s_I_KH_P_I_KH_T[i][j] + s_K_R_K_T[i][j];
        }
    }

    /* Ensure covariance symmetry for numerical stability */
    symmetrize_5x5(ekf->covariance);

    return SD_OK;
}
