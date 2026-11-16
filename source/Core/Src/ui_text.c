#include "ui_text.h"
#include "ui_font_zhengge16.h"

#include <stdbool.h>
#include <stddef.h>

static const uint8_t *UiText_FindGlyph(uint32_t codepoint, uint8_t *width,
                                       uint8_t *advance)
{
    uint16_t low = 0U;
    uint16_t high = UI_FONT_ZHENGGE16_COUNT;

    while (low < high)
    {
        uint16_t middle = (uint16_t)(low + ((high - low) / 2U));
        uint16_t candidate = ui_font_zhengge16_codepoints[middle];

        if (candidate == codepoint)
        {
            *width = ui_font_zhengge16_widths[middle];
            *advance = ui_font_zhengge16_advances[middle];
            return &ui_font_zhengge16_bitmaps[ui_font_zhengge16_offsets[middle]];
        }
        if (candidate < codepoint)
        {
            low = (uint16_t)(middle + 1U);
        }
        else
        {
            high = middle;
        }
    }
    return NULL;
}

static uint32_t UiText_DecodeUtf8(const uint8_t **cursor)
{
    const uint8_t *p = *cursor;
    uint32_t value;

    if (p[0] < 0x80U)
    {
        *cursor = p + 1;
        return p[0];
    }
    if ((p[0] & 0xE0U) == 0xC0U && (p[1] & 0xC0U) == 0x80U)
    {
        value = ((uint32_t)(p[0] & 0x1FU) << 6U) | (uint32_t)(p[1] & 0x3FU);
        *cursor = p + 2;
        return value;
    }
    if ((p[0] & 0xF0U) == 0xE0U && p[1] != 0U && p[2] != 0U &&
        (p[1] & 0xC0U) == 0x80U && (p[2] & 0xC0U) == 0x80U)
    {
        value = ((uint32_t)(p[0] & 0x0FU) << 12U) |
                ((uint32_t)(p[1] & 0x3FU) << 6U) |
                (uint32_t)(p[2] & 0x3FU);
        *cursor = p + 3;
        return value;
    }
    if ((p[0] & 0xF8U) == 0xF0U && p[1] != 0U && p[2] != 0U && p[3] != 0U &&
        (p[1] & 0xC0U) == 0x80U && (p[2] & 0xC0U) == 0x80U &&
        (p[3] & 0xC0U) == 0x80U)
    {
        value = ((uint32_t)(p[0] & 0x07U) << 18U) |
                ((uint32_t)(p[1] & 0x3FU) << 12U) |
                ((uint32_t)(p[2] & 0x3FU) << 6U) |
                (uint32_t)(p[3] & 0x3FU);
        *cursor = p + 4;
        return value;
    }

    *cursor = p + 1;
    return (uint32_t)'?';
}

uint16_t UiText_Width(const char *text)
{
    const uint8_t *cursor = (const uint8_t *)text;
    uint16_t width = 0U;

    while (*cursor != 0U)
    {
        uint8_t glyph_width = 0U;
        uint8_t advance = 0U;
        uint32_t codepoint = UiText_DecodeUtf8(&cursor);

        if (UiText_FindGlyph(codepoint, &glyph_width, &advance) == NULL)
        {
            (void)UiText_FindGlyph((uint32_t)'?', &glyph_width, &advance);
        }
        width = (uint16_t)(width + advance);
    }
    return width;
}

void UiText_Draw(int16_t x, int16_t y, const char *text,
                 ui_color_t foreground, ui_color_t background)
{
    const uint8_t *cursor = (const uint8_t *)text;

    while (*cursor != 0U)
    {
        uint8_t glyph_width = 0U;
        uint8_t advance = 0U;
        uint32_t codepoint = UiText_DecodeUtf8(&cursor);
        const uint8_t *glyph = UiText_FindGlyph(codepoint, &glyph_width, &advance);

        if (glyph == NULL)
        {
            glyph = UiText_FindGlyph((uint32_t)'?', &glyph_width, &advance);
        }
        if (glyph != NULL && glyph_width != 0U)
        {
            PlatformDisplay_DrawBitmap(x, y, glyph_width,
                                       UI_FONT_ZHENGGE16_HEIGHT, glyph,
                                       foreground, background);
        }
        x = (int16_t)(x + advance);
    }
}

void UiText_DrawCentered(int16_t center_x, int16_t center_y, const char *text,
                         ui_color_t foreground, ui_color_t background)
{
    uint16_t width = UiText_Width(text);
    int16_t x = (int16_t)(center_x - ((int16_t)width / 2));
    int16_t y = (int16_t)(center_y - (UI_FONT_ZHENGGE16_HEIGHT / 2));

    UiText_Draw(x, y, text, foreground, background);
}
