#ifndef APP_PLATFORM_H
#define APP_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

#include "app_motion.h"
#include "app_params.h"

typedef enum
{
    UI_COLOR_BLACK = 0,
    UI_COLOR_WHITE,
    UI_COLOR_RED
} ui_color_t;

bool Platform_Init(void);

void PlatformDisplay_FillRect(int16_t x, int16_t y, uint8_t width,
                              uint8_t height, ui_color_t color);
void PlatformDisplay_DrawBitmap(int16_t x, int16_t y, uint8_t width,
                                uint8_t height, const uint8_t *bits,
                                ui_color_t foreground, ui_color_t background);
void PlatformDisplay_Present(void);

bool PlatformStorage_Load(app_params_t *params);
bool PlatformStorage_Save(const app_params_t *params);

bool PlatformMotion_Start(app_motion_dir_t direction, uint32_t initial_psc);
void PlatformMotion_SetPsc(uint32_t psc);
void PlatformMotion_Stop(void);

uint32_t PlatformIrqSave(void);
void PlatformIrqRestore(uint32_t state);

#endif
