#include "oled.h"

#include "i2c.h"

#include <string.h>

#define OLED_PAGE_COUNT       (OLED_HEIGHT / 8U)
#define OLED_I2C_TIMEOUT_MS   50U

static uint8_t oled_address;
static bool oled_ready;
static uint8_t framebuffer[OLED_WIDTH * OLED_PAGE_COUNT];

static bool OLED_WriteCommand(uint8_t command)
{
  uint8_t packet[2] = {0x00U, command};

  return HAL_I2C_Master_Transmit(&hi2c1,
                                 oled_address,
                                 packet,
                                 sizeof(packet),
                                 OLED_I2C_TIMEOUT_MS) == HAL_OK;
}

static bool OLED_WritePage(const uint8_t *data)
{
  uint8_t packet[OLED_WIDTH + 1U];

  packet[0] = 0x40U;
  memcpy(&packet[1], data, OLED_WIDTH);
  return HAL_I2C_Master_Transmit(&hi2c1,
                                 oled_address,
                                 packet,
                                 sizeof(packet),
                                 OLED_I2C_TIMEOUT_MS) == HAL_OK;
}

static const uint8_t *OLED_GetGlyph(char character)
{
  static const uint8_t blank[5] = {0, 0, 0, 0, 0};
  static const uint8_t dash[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
  static const uint8_t colon[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
  static const uint8_t period[5] = {0x00, 0x60, 0x60, 0x00, 0x00};
  static const uint8_t slash[5] = {0x20, 0x10, 0x08, 0x04, 0x02};
  static const uint8_t digits[10][5] = {
      {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},
      {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
      {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},
      {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
      {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E}
  };
  static const uint8_t letters[26][5] = {
      {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36},
      {0x3E,0x41,0x41,0x41,0x22}, {0x7F,0x41,0x41,0x22,0x1C},
      {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},
      {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F},
      {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01},
      {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
      {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F},
      {0x3E,0x41,0x41,0x41,0x3E}, {0x7F,0x09,0x09,0x09,0x06},
      {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},
      {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01},
      {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F},
      {0x3F,0x40,0x38,0x40,0x3F}, {0x63,0x14,0x08,0x14,0x63},
      {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}
  };

  if ((character >= 'a') && (character <= 'z'))
  {
    character = (char)(character - 'a' + 'A');
  }
  if ((character >= '0') && (character <= '9'))
  {
    return digits[(uint8_t)(character - '0')];
  }
  if ((character >= 'A') && (character <= 'Z'))
  {
    return letters[(uint8_t)(character - 'A')];
  }
  if (character == '-')
  {
    return dash;
  }
  if (character == ':')
  {
    return colon;
  }
  if (character == '.')
  {
    return period;
  }
  if (character == '/')
  {
    return slash;
  }
  return blank;
}

bool OLED_Init(void)
{
  static const uint8_t candidates[] = {
      (uint8_t)(0x3CU << 1U),
      (uint8_t)(0x3DU << 1U)
  };
  static const uint8_t init_commands[] = {
      0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
      0x8D, 0x14, 0x20, 0x02, 0xA1, 0xC8, 0xDA, 0x12,
      0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6,
      0x2E, 0xAF
  };
  uint8_t index;

  oled_ready = false;
  oled_address = 0U;
  for (index = 0U; index < sizeof(candidates); ++index)
  {
    if (HAL_I2C_IsDeviceReady(&hi2c1,
                              candidates[index],
                              3U,
                              OLED_I2C_TIMEOUT_MS) == HAL_OK)
    {
      oled_address = candidates[index];
      break;
    }
  }
  if (oled_address == 0U)
  {
    return false;
  }

  HAL_Delay(50U);
  for (index = 0U; index < sizeof(init_commands); ++index)
  {
    if (!OLED_WriteCommand(init_commands[index]))
    {
      return false;
    }
  }

  OLED_Clear();
  oled_ready = OLED_Update();
  return oled_ready;
}

bool OLED_IsReady(void)
{
  return oled_ready;
}

uint8_t OLED_GetAddress(void)
{
  return (uint8_t)(oled_address >> 1U);
}

void OLED_Clear(void)
{
  memset(framebuffer, 0, sizeof(framebuffer));
}

void OLED_SetPixel(uint8_t x, uint8_t y, bool on)
{
  uint16_t index;
  uint8_t mask;

  if ((x >= OLED_WIDTH) || (y >= OLED_HEIGHT))
  {
    return;
  }

  index = (uint16_t)x + ((uint16_t)(y >> 3U) * OLED_WIDTH);
  mask = (uint8_t)(1U << (y & 7U));
  if (on)
  {
    framebuffer[index] |= mask;
  }
  else
  {
    framebuffer[index] &= (uint8_t)~mask;
  }
}

void OLED_DrawTextScaled(uint8_t x,
                         uint8_t y,
                         const char *text,
                         uint8_t scale)
{
  if ((text == NULL) || (scale == 0U))
  {
    return;
  }

  while (*text != '\0')
  {
    const uint8_t *glyph;
    uint8_t column;
    uint8_t row;
    uint8_t sx;
    uint8_t sy;
    const uint16_t next_x = (uint16_t)x + (uint16_t)(6U * scale);

    if (((uint16_t)x + (uint16_t)(5U * scale)) > OLED_WIDTH)
    {
      break;
    }

    glyph = OLED_GetGlyph(*text++);
    for (column = 0U; column < 5U; ++column)
    {
      for (row = 0U; row < 7U; ++row)
      {
        if ((glyph[column] & (1U << row)) == 0U)
        {
          continue;
        }
        for (sx = 0U; sx < scale; ++sx)
        {
          for (sy = 0U; sy < scale; ++sy)
          {
            OLED_SetPixel((uint8_t)(x + column * scale + sx),
                          (uint8_t)(y + row * scale + sy),
                          true);
          }
        }
      }
    }
    x = (uint8_t)next_x;
  }
}

void OLED_DrawText(uint8_t x, uint8_t y, const char *text)
{
  OLED_DrawTextScaled(x, y, text, 1U);
}

bool OLED_Update(void)
{
  uint8_t page;

  if (oled_address == 0U)
  {
    return false;
  }

  for (page = 0U; page < OLED_PAGE_COUNT; ++page)
  {
    if (!OLED_WriteCommand((uint8_t)(0xB0U + page)) ||
        !OLED_WriteCommand(0x00U) ||
        !OLED_WriteCommand(0x10U) ||
        !OLED_WritePage(&framebuffer[(uint16_t)page * OLED_WIDTH]))
    {
      oled_ready = false;
      return false;
    }
  }
  oled_ready = true;
  return true;
}
