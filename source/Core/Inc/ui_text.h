#ifndef UI_TEXT_H
#define UI_TEXT_H

#include <stdint.h>

#include "app_platform.h"

uint16_t UiText_Width(const char *text);
void UiText_Draw(int16_t x, int16_t y, const char *text,
                 ui_color_t foreground, ui_color_t background);
void UiText_DrawCentered(int16_t center_x, int16_t center_y, const char *text,
                         ui_color_t foreground, ui_color_t background);

#endif
