/**
 * @file      EKF.c
 * @brief     Extended Kalman Filter implementation
 * @author    joelin-CN
 * @version   1.0.0
 * @date      2026-07-17
 */

#include "../inc/EKF.h"
#include "../inc/utils.h"
#include <math.h>
#include <string.h>

static void matrix_multiply_5x5(float A[5][5], float B[5][5], float result[5][5]) {
    size_t i, j, k;
    float temp[5][5];
    
    for (i = 0; i < 5; ++i) {
        for (j = 0; j < 5; ++j) {
            temp[i][j] = 0.0f;
            for (k = 0; k < 5; ++k) {
                temp[i][j] += A[i][k] * B[k][j];
            }
        }
    }
    
    memcpy(result, temp, sizeof(temp));
}

static void matrix_transpose_5x5(float A[5][5], float result[5][5]) {
    size_t i, j;
    float temp[5][5];
    
    for (i = 0; i < 5; ++i) {
        for (j = 0; j < 5; ++j) {
            temp[i][j] = A[j][i];
        }
    }
    
    memcpy(result, temp, sizeof(temp));
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

void ekf_predict(ekf_t *ekf, float dt) {
    float x = ekf->state[0];
    float y = ekf->state[1];
    float theta = ekf->state[2];
    float v = ekf->state[3];
    float omega = ekf->state[4];
    float cos_theta = cosf(theta);
    float sin_theta = sinf(theta);
    float F[5][5];
    float F_T[5][5];
    float P_temp[5][5];
    size_t i, j;
    
    ekf->state[0] = x + v * cos_theta * dt;
    ekf->state[1] = y + v * sin_theta * dt;
    ekf->state[2] = sd_normalize_angle(theta + omega * dt);
    ekf->state[3] = v;
    ekf->state[4] = omega;
    
    memset(F, 0, sizeof(F));
    for (i = 0; i < 5; ++i) {
        F[i][i] = 1.0f;
    }
    F[0][2] = -v * sin_theta * dt;
    F[0][3] = cos_theta * dt;
    F[1][2] = v * cos_theta * dt;
    F[1][3] = sin_theta * dt;
    F[2][4] = dt;
    
    matrix_transpose_5x5(F, F_T);
    matrix_multiply_5x5(ekf->covariance, F_T, P_temp);
    matrix_multiply_5x5(F, P_temp, ekf->covariance);
    
    for (i = 0; i < SD_EKF_STATE_COUNT; ++i) {
        for (j = 0; j < SD_EKF_STATE_COUNT; ++j) {
            ekf->covariance[i][j] += ekf->process_noise[i][j];
        }
    }
}

sd_status_t ekf_update(ekf_t *ekf, const float observation[SD_EKF_OBSERVATION_COUNT]) {
    float H[3][5];
    float H_T[5][3];
    float y[3];
    float S[3][3];
    float S_inv[3][3];
    float P_H_T[5][3];
    float K[5][3];
    float I_KH[5][5];
    float I_KH_T[5][5];
    float I_KH_P[5][5];
    float I_KH_P_I_KH_T[5][5];
    float K_R[5][3];
    float K_R_K_T[5][5];
    float det;
    size_t i, j, k;
    
    memset(H, 0, sizeof(H));
    H[0][3] = 1.0f;
    H[1][4] = 1.0f;
    H[2][4] = 1.0f;
    
    y[0] = observation[0] - ekf->state[3];
    y[1] = observation[1] - ekf->state[4];
    y[2] = observation[2] - ekf->state[4];
    
    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 5; ++j) {
            H_T[j][i] = H[i][j];
        }
    }
    
    for (i = 0; i < 5; ++i) {
        for (j = 0; j < 3; ++j) {
            P_H_T[i][j] = 0.0f;
            for (k = 0; k < 5; ++k) {
                P_H_T[i][j] += ekf->covariance[i][k] * H_T[k][j];
            }
        }
    }
    
    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j) {
            S[i][j] = ekf->observation_noise[i][j];
            for (k = 0; k < 5; ++k) {
                S[i][j] += H[i][k] * P_H_T[k][j];
            }
        }
    }
    
    det = S[0][0] * (S[1][1] * S[2][2] - S[1][2] * S[2][1])
        - S[0][1] * (S[1][0] * S[2][2] - S[1][2] * S[2][0])
        + S[0][2] * (S[1][0] * S[2][1] - S[1][1] * S[2][0]);
    
    if (!isfinite(det) || fabsf(det) < 1e-10f) {
        return SD_ERR_NUMERIC;
    }
    
    S_inv[0][0] = (S[1][1] * S[2][2] - S[1][2] * S[2][1]) / det;
    S_inv[0][1] = (S[0][2] * S[2][1] - S[0][1] * S[2][2]) / det;
    S_inv[0][2] = (S[0][1] * S[1][2] - S[0][2] * S[1][1]) / det;
    S_inv[1][0] = (S[1][2] * S[2][0] - S[1][0] * S[2][2]) / det;
    S_inv[1][1] = (S[0][0] * S[2][2] - S[0][2] * S[2][0]) / det;
    S_inv[1][2] = (S[0][2] * S[1][0] - S[0][0] * S[1][2]) / det;
    S_inv[2][0] = (S[1][0] * S[2][1] - S[1][1] * S[2][0]) / det;
    S_inv[2][1] = (S[0][1] * S[2][0] - S[0][0] * S[2][1]) / det;
    S_inv[2][2] = (S[0][0] * S[1][1] - S[0][1] * S[1][0]) / det;
    
    for (i = 0; i < 5; ++i) {
        for (j = 0; j < 3; ++j) {
            K[i][j] = 0.0f;
            for (k = 0; k < 3; ++k) {
                K[i][j] += P_H_T[i][k] * S_inv[k][j];
            }
        }
    }
    
    for (i = 0; i < 5; ++i) {
        float correction = 0.0f;
        for (j = 0; j < 3; ++j) {
            correction += K[i][j] * y[j];
        }
        ekf->state[i] += correction;
    }
    
    ekf->state[2] = sd_normalize_angle(ekf->state[2]);
    
    memset(I_KH, 0, sizeof(I_KH));
    for (i = 0; i < 5; ++i) {
        I_KH[i][i] = 1.0f;
        for (j = 0; j < 5; ++j) {
            for (k = 0; k < 3; ++k) {
                I_KH[i][j] -= K[i][k] * H[k][j];
            }
        }
    }
    
    matrix_transpose_5x5(I_KH, I_KH_T);
    matrix_multiply_5x5(I_KH, ekf->covariance, I_KH_P);
    matrix_multiply_5x5(I_KH_P, I_KH_T, I_KH_P_I_KH_T);
    
    for (i = 0; i < 5; ++i) {
        for (j = 0; j < 3; ++j) {
            K_R[i][j] = 0.0f;
            for (k = 0; k < 3; ++k) {
                K_R[i][j] += K[i][k] * ekf->observation_noise[k][j];
            }
        }
    }
    
    for (i = 0; i < 5; ++i) {
        for (j = 0; j < 5; ++j) {
            K_R_K_T[i][j] = 0.0f;
            for (k = 0; k < 3; ++k) {
                K_R_K_T[i][j] += K_R[i][k] * K[j][k];
            }
        }
    }
    
    for (i = 0; i < 5; ++i) {
        for (j = 0; j < 5; ++j) {
            ekf->covariance[i][j] = I_KH_P_I_KH_T[i][j] + K_R_K_T[i][j];
        }
    }
    
    symmetrize_5x5(ekf->covariance);
    
    return SD_OK;
}
