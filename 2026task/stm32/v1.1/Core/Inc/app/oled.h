#ifndef OLED_H
#define OLED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#define OLED_WIDTH   128U
#define OLED_HEIGHT   64U

/** Detect and initialize an SSD1306 OLED at I2C address 0x3C or 0x3D. */
bool OLED_Init(void);

/** Return true after OLED_Init has completed successfully. */
bool OLED_IsReady(void);

/** Return the detected 7-bit I2C address, or zero before initialization. */
uint8_t OLED_GetAddress(void);

/** Clear only the RAM framebuffer; call OLED_Update to show the change. */
void OLED_Clear(void);

void OLED_SetPixel(uint8_t x, uint8_t y, bool on);
void OLED_DrawText(uint8_t x, uint8_t y, const char *text);
void OLED_DrawTextScaled(uint8_t x, uint8_t y, const char *text,
                         uint8_t scale);

/** Transfer the complete framebuffer to the OLED. */
bool OLED_Update(void);

#ifdef __cplusplus
}
#endif

#endif /* OLED_H */
