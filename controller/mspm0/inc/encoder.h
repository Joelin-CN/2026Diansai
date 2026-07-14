#ifndef ENCODER_H_
#define ENCODER_H_

#include <stdint.h>

typedef enum {
    ENCODER_M1 = 0,
    ENCODER_M2,
    ENCODER_M3,
    ENCODER_M4,
    ENCODER_COUNT
} Encoder_Id;

void Encoder_Init(void);
int32_t Encoder_GetCount(Encoder_Id encoder);
void Encoder_ResetCount(Encoder_Id encoder);

#endif /* ENCODER_H_ */
