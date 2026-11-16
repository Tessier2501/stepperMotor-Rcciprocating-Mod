#include "app_view.h"
#include "app_platform.h"
#include "ui_text.h"

#define VIEW_ROW_X       2
#define VIEW_ROW_WIDTH   76U
#define VIEW_ROW_HEIGHT  50U
#define VIEW_CENTER_X    40

static const uint16_t s_row_tops[3] = {3U, 55U, 107U};

static app_param_index_t AppView_OffsetParam(app_param_index_t index, int8_t delta)
{
    int16_t next = (int16_t)index + delta;

    if (next < 0)
    {
        next = (int16_t)(APP_PARAM_COUNT - 1);
    }
    else if (next >= (int16_t)APP_PARAM_COUNT)
    {
        next = 0;
    }
    return (app_param_index_t)next;
}

static void AppView_DrawOutline(int16_t x, int16_t y, uint8_t width,
                                uint8_t height, ui_color_t color)
{
    PlatformDisplay_FillRect(x, y, width, 1U, color);
    PlatformDisplay_FillRect(x, (int16_t)(y + height - 1), width, 1U, color);
    PlatformDisplay_FillRect(x, y, 1U, height, color);
    PlatformDisplay_FillRect((int16_t)(x + width - 1), y, 1U, height, color);
}

static void AppView_FormatValue(int32_t value, char *buffer)
{
    char digits[12];
    uint8_t count = 0U;
    uint8_t position = 0U;
    uint32_t magnitude = (value < 0) ? (uint32_t)(-(int64_t)value) : (uint32_t)value;

    do
    {
        digits[count++] = (char)('0' + (magnitude % 10U));
        magnitude /= 10U;
    } while (magnitude != 0U);

    if (value < 0)
    {
        buffer[position++] = '-';
    }
    while (count > 0U)
    {
        buffer[position++] = digits[--count];
    }
    buffer[position] = '\0';
}

static void AppView_FormatLabelValue(const char *label, int32_t value,
                                     char *buffer)
{
    char value_text[12];
    uint8_t position = 0U;

    AppView_FormatValue(value, value_text);
    while (*label != '\0')
    {
        buffer[position++] = *label++;
    }
    for (uint8_t i = 0U; value_text[i] != '\0'; i++)
    {
        buffer[position++] = value_text[i];
    }
    buffer[position] = '\0';
}

static void AppView_DrawStartup(void)
{
    static const char *const buttons[3] = {"UP", "DOWN", "TRIG"};
    static const char *const descriptions[3] = {"手动上行", "手动下行", "长按归零"};
    uint8_t row;

    for (row = 0U; row < 3U; row++)
    {
        int16_t top = (int16_t)s_row_tops[row];
        AppView_DrawOutline(VIEW_ROW_X, top, VIEW_ROW_WIDTH, VIEW_ROW_HEIGHT,
                            UI_COLOR_RED);
        UiText_DrawCentered(VIEW_CENTER_X, (int16_t)(top + 15), buttons[row],
                            UI_COLOR_RED, UI_COLOR_BLACK);
        UiText_DrawCentered(VIEW_CENTER_X, (int16_t)(top + 35), descriptions[row],
                            UI_COLOR_RED, UI_COLOR_BLACK);
    }
}

static void AppView_DrawMain(const app_view_state_t *state)
{
    app_param_index_t previous = AppView_OffsetParam(state->selected_param, -1);
    app_param_index_t next = AppView_OffsetParam(state->selected_param, 1);
    app_param_index_t params[3] = {previous, state->selected_param, next};
    uint8_t row;

    for (row = 0U; row < 3U; row++)
    {
        int16_t top = (int16_t)s_row_tops[row];
        bool selected = (row == 1U);
        char value[16];
        ui_color_t foreground = selected ? UI_COLOR_BLACK : UI_COLOR_WHITE;
        ui_color_t background = selected ? UI_COLOR_WHITE : UI_COLOR_BLACK;

        if (selected)
        {
            PlatformDisplay_FillRect(VIEW_ROW_X, top, VIEW_ROW_WIDTH,
                                     VIEW_ROW_HEIGHT, UI_COLOR_WHITE);
        }
        else
        {
            AppView_DrawOutline(VIEW_ROW_X, top, VIEW_ROW_WIDTH,
                                VIEW_ROW_HEIGHT, UI_COLOR_WHITE);
        }

        AppView_FormatValue(AppParams_GetValue(&state->params, params[row]),
                            value);
        UiText_DrawCentered(VIEW_CENTER_X, (int16_t)(top + 15),
                            AppParams_Label(params[row]), foreground, background);
        UiText_DrawCentered(VIEW_CENTER_X, (int16_t)(top + 35), value,
                            foreground, background);
    }
}

static void AppView_DrawEdit(const app_view_state_t *state)
{
    char old_value[24];
    char new_value[24];

    PlatformDisplay_FillRect(1, 3, 78U, 154U, UI_COLOR_WHITE);
    UiText_DrawCentered(VIEW_CENTER_X, 22, AppParams_Label(state->edit_param),
                        UI_COLOR_BLACK, UI_COLOR_WHITE);
    AppView_FormatLabelValue("旧: ",
                             AppParams_GetValue(&state->edit_old, state->edit_param),
                             old_value);
    AppView_FormatLabelValue("新: ",
                             AppParams_GetValue(&state->edit_new, state->edit_param),
                             new_value);
    UiText_DrawCentered(VIEW_CENTER_X, 55, old_value,
                        UI_COLOR_BLACK, UI_COLOR_WHITE);
    UiText_DrawCentered(VIEW_CENTER_X, 88, new_value,
                        UI_COLOR_BLACK, UI_COLOR_WHITE);
    UiText_DrawCentered(VIEW_CENTER_X, 121, AppParams_Unit(state->edit_param),
                        UI_COLOR_BLACK, UI_COLOR_WHITE);
}

void AppView_Render(const app_view_state_t *state)
{
    PlatformDisplay_FillRect(0, 0, APP_VIEW_WIDTH, APP_VIEW_HEIGHT,
                             UI_COLOR_BLACK);

    switch (state->screen)
    {
        case APP_VIEW_SCREEN_UNHOMED:
            AppView_DrawStartup();
            break;
        case APP_VIEW_SCREEN_MAIN:
            AppView_DrawMain(state);
            break;
        case APP_VIEW_SCREEN_EDIT:
            AppView_DrawEdit(state);
            break;
        default:
            break;
    }

    PlatformDisplay_Present();
}
