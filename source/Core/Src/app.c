#include "app.h"
#include "app_motion.h"
#include "app_params.h"
#include "main.h"

#include <stdbool.h>
#include <stdint.h>

#define APP_BUTTON_DEBOUNCE_MS          20U    /* placeholder */
#define APP_TRIG_LONG_PRESS_MS          1000U  /* placeholder */
#define APP_JOG_LONG_PRESS_MS           300U   /* placeholder */
#define APP_EDIT_REPEAT_DELAY_MS        500U   /* placeholder */
#define APP_EDIT_REPEAT_PERIOD_MS       100U   /* placeholder */
#define APP_HOME_SPEED_PPS              405U   /* placeholder */
#define APP_EDIT_SPEED_PPS              405U   /* placeholder */
#define APP_EDIT_DELAY_STEP_MS          10U    /* placeholder */
#define APP_EDIT_SPEED_STEP_PPS         10U    /* placeholder */
#define APP_EDIT_POS_STEP_PULSES        10     /* placeholder */
#define APP_HOME_RELEASE_EXTRA_PULSES   100U   /* placeholder */
#define APP_HOME_RELEASE_MAX_PULSES     2000U  /* placeholder */
#define APP_HOME_SEARCH_MAX_PULSES      12000U /* placeholder */
#define APP_HOME_RELEASE_TIMEOUT_MS     10000U /* placeholder */
#define APP_HOME_SEARCH_TIMEOUT_MS      30000U /* placeholder */
#define APP_HOME_TOP_TIMEOUT_MS         60000U /* placeholder */
#define APP_AUTO_MOVE_TIMEOUT_MS        60000U /* placeholder */

typedef enum
{
    APP_STATE_MANUAL = 0,
    APP_STATE_HOMING,
    APP_STATE_READY,
    APP_STATE_AUTO
} app_state_t;

typedef enum
{
    APP_PARAM_DELAY = 0,
    APP_PARAM_UP_SPEED,
    APP_PARAM_DOWN_SPEED,
    APP_PARAM_BOTTOM,
    APP_PARAM_TOP,
    APP_PARAM_COUNT
} app_param_index_t;

typedef enum
{
    APP_HOME_RELEASE = 0,
    APP_HOME_RELEASE_EXTRA,
    APP_HOME_SEARCH,
    APP_HOME_MOVE_TO_TOP
} app_home_phase_t;

typedef enum
{
    APP_AUTO_MOVE_TO_TOP = 0,
    APP_AUTO_MOVE_DOWN,
    APP_AUTO_DELAY,
    APP_AUTO_MOVE_UP
} app_auto_phase_t;

typedef struct
{
    bool raw;
    bool stable;
    uint32_t changed_ms;
    uint32_t press_edge_ms;
} app_debounce_t;

static app_state_t s_state;
static bool s_homed;
static bool s_homing_fault;
static app_home_phase_t s_home_phase;
static uint32_t s_home_phase_start_ms;
static app_auto_phase_t s_auto_phase;
static uint32_t s_auto_phase_start_ms;
static uint32_t s_auto_wait_until_ms;

static app_debounce_t s_up;
static app_debounce_t s_down;
static app_debounce_t s_trig;
static app_debounce_t s_origin;
static bool s_trig_was_pressed;
static bool s_trig_long_fired;
static uint32_t s_trig_press_ms;
static app_state_t s_trig_press_state;
static bool s_trig_press_edit;

static bool s_up_was_pressed;
static bool s_down_was_pressed;
static bool s_up_long_fired;
static bool s_down_long_fired;
static uint32_t s_up_press_ms;
static uint32_t s_down_press_ms;
static app_state_t s_up_press_state;
static app_state_t s_down_press_state;
static bool s_up_press_edit;
static bool s_down_press_edit;

static uint8_t s_selected_param;
static bool s_edit_active;
static uint8_t s_edit_index;
static app_params_t s_edit_params;
static bool s_save_pending;
static bool s_edit_error;
static bool s_edit_move_failed;
static int32_t s_edit_move_target;
static bool s_edit_move_target_valid;
static int8_t s_edit_repeat_direction;
static bool s_edit_repeat_started;
static uint32_t s_edit_repeat_start_ms;
static uint32_t s_edit_repeat_last_ms;

static bool App_ReadUp(void)
{
    return HAL_GPIO_ReadPin(Up_GPIO_Port, Up_Pin) == GPIO_PIN_RESET;
}

static bool App_ReadDown(void)
{
    return HAL_GPIO_ReadPin(Down_GPIO_Port, Down_Pin) == GPIO_PIN_RESET;
}

static bool App_ReadTrig(void)
{
    return HAL_GPIO_ReadPin(Trig_GPIO_Port, Trig_Pin) == GPIO_PIN_RESET;
}

static void App_DebounceInit(app_debounce_t *input, bool raw, uint32_t now)
{
    input->raw = raw;
    input->stable = raw;
    input->changed_ms = now;
    input->press_edge_ms = raw ? now : 0U;
}

static void App_DebounceUpdate(app_debounce_t *input, bool raw, uint32_t now)
{
    if (raw != input->raw)
    {
        if (raw)
        {
            input->press_edge_ms = now;
        }
        input->raw = raw;
        input->changed_ms = now;
    }
    if (raw != input->stable &&
        (uint32_t)(now - input->changed_ms) >= APP_BUTTON_DEBOUNCE_MS)
    {
        input->stable = raw;
    }
}

static void App_UpdateInputs(uint32_t now)
{
    App_DebounceUpdate(&s_up, App_ReadUp(), now);
    App_DebounceUpdate(&s_down, App_ReadDown(), now);
    App_DebounceUpdate(&s_trig, App_ReadTrig(), now);
    App_DebounceUpdate(&s_origin, AppMotion_IsOriginActive(), now);
}

static bool App_OriginStableActive(void)
{
    return s_origin.stable;
}

static bool App_TrigPressMatches(void)
{
    return s_trig_press_state == s_state && s_trig_press_edit == s_edit_active;
}

static bool App_UpPressMatches(void)
{
    return s_up_press_state == s_state && s_up_press_edit == s_edit_active;
}

static bool App_DownPressMatches(void)
{
    return s_down_press_state == s_state && s_down_press_edit == s_edit_active;
}

static int32_t App_ClampI32(int64_t value)
{
    if (value < INT32_MIN)
    {
        return INT32_MIN;
    }
    if (value > INT32_MAX)
    {
        return INT32_MAX;
    }
    return (int32_t)value;
}

static void App_StartAuto(void);

static void App_FinishEdit(void)
{
    s_edit_active = false;
    s_save_pending = false;
    s_edit_error = false;
    s_edit_move_failed = false;
    s_edit_move_target_valid = false;
    s_edit_repeat_direction = 0;
}

static void App_FailHoming(void)
{
    AppMotion_Stop();
    App_FinishEdit();
    s_homed = false;
    s_homing_fault = true;
    s_state = APP_STATE_MANUAL;
}

static void App_FailMotion(void)
{
    AppMotion_Stop();
    App_FinishEdit();
    s_homed = false;
    s_homing_fault = true;
    s_state = APP_STATE_MANUAL;
}

static void App_StartHoming(void)
{
    AppMotion_Stop();
    App_FinishEdit();
    s_homed = false;
    s_homing_fault = false;
    s_state = APP_STATE_HOMING;
    s_home_phase_start_ms = HAL_GetTick();

    if (App_OriginStableActive())
    {
        s_home_phase = APP_HOME_RELEASE;
        if (!AppMotion_StartJog(APP_MOTION_DIR_DOWN, APP_HOME_SPEED_PPS))
        {
            App_FailHoming();
        }
    }
    else
    {
        s_home_phase = APP_HOME_SEARCH;
        if (!AppMotion_StartJog(APP_MOTION_DIR_UP, APP_HOME_SPEED_PPS))
        {
            App_FailHoming();
        }
    }
}

static void App_ProcessHoming(uint32_t now)
{
    switch (s_home_phase)
    {
        case APP_HOME_RELEASE:
            if (App_OriginStableActive())
            {
                if (AppMotion_GetPulseCount() >= APP_HOME_RELEASE_MAX_PULSES ||
                    (uint32_t)(now - s_home_phase_start_ms) >= APP_HOME_RELEASE_TIMEOUT_MS ||
                    !AppMotion_IsMoving())
                {
                    App_FailHoming();
                }
            }
            else
            {
                AppMotion_Stop();
                if (!AppMotion_StartRelative(APP_MOTION_DIR_DOWN,
                                             APP_HOME_RELEASE_EXTRA_PULSES,
                                             APP_HOME_SPEED_PPS))
                {
                    App_FailHoming();
                    return;
                }
                s_home_phase = APP_HOME_RELEASE_EXTRA;
                s_home_phase_start_ms = now;
            }
            break;

        case APP_HOME_RELEASE_EXTRA:
            if (AppMotion_IsMoving())
            {
                if ((uint32_t)(now - s_home_phase_start_ms) >= APP_HOME_RELEASE_TIMEOUT_MS)
                {
                    App_FailHoming();
                }
                break;
            }
            if (AppMotion_TakeDone())
            {
                if (!AppMotion_StartJog(APP_MOTION_DIR_UP, APP_HOME_SPEED_PPS))
                {
                    App_FailHoming();
                    return;
                }
                s_home_phase = APP_HOME_SEARCH;
                s_home_phase_start_ms = now;
            }
            else
            {
                App_FailHoming();
            }
            break;

        case APP_HOME_SEARCH:
            if (App_OriginStableActive())
            {
                const app_params_t *params = AppParams_Get();
                AppMotion_Stop();
                AppMotion_SetPosition(0);
                if (AppParams_IsUsable())
                {
                    if (!AppMotion_StartMoveTo(params->top_pos, APP_HOME_SPEED_PPS))
                    {
                        App_FailHoming();
                        return;
                    }
                    s_home_phase = APP_HOME_MOVE_TO_TOP;
                    s_home_phase_start_ms = now;
                }
                else
                {
                    s_homed = true;
                    s_state = APP_STATE_READY;
                }
            }
            else if (AppMotion_GetPulseCount() >= APP_HOME_SEARCH_MAX_PULSES ||
                     (uint32_t)(now - s_home_phase_start_ms) >= APP_HOME_SEARCH_TIMEOUT_MS ||
                     !AppMotion_IsMoving())
            {
                App_FailHoming();
            }
            break;

        case APP_HOME_MOVE_TO_TOP:
            if (AppMotion_IsMoving())
            {
                if ((uint32_t)(now - s_home_phase_start_ms) >= APP_HOME_TOP_TIMEOUT_MS)
                {
                    App_FailHoming();
                }
                break;
            }
            if (AppMotion_TakeDone())
            {
                s_homed = true;
                s_state = APP_STATE_READY;
            }
            else
            {
                App_FailHoming();
            }
            break;

        default:
            App_FailHoming();
            break;
    }
}

static void App_StartAuto(void)
{
    const app_params_t *params = AppParams_Get();

    if (!s_homed || !AppParams_IsUsable() || s_edit_active)
    {
        return;
    }

    AppMotion_Stop();
    s_state = APP_STATE_AUTO;
    s_auto_phase = APP_AUTO_MOVE_TO_TOP;
    s_auto_phase_start_ms = HAL_GetTick();

    if (!AppMotion_StartMoveTo(params->top_pos,
                               (params->top_pos > AppMotion_GetPosition())
                                   ? params->up_speed_pps
                                   : params->down_speed_pps))
    {
        App_FailMotion();
    }
}

static void App_ProcessAuto(void)
{
    const app_params_t *params = AppParams_Get();

    if (!AppParams_IsUsable())
    {
        App_FailMotion();
        return;
    }

    if (s_auto_phase != APP_AUTO_DELAY &&
        (uint32_t)(HAL_GetTick() - s_auto_phase_start_ms) >= APP_AUTO_MOVE_TIMEOUT_MS)
    {
        App_FailMotion();
        return;
    }

    switch (s_auto_phase)
    {
        case APP_AUTO_MOVE_TO_TOP:
            if (AppMotion_IsMoving())
            {
                break;
            }
            if (AppMotion_TakeDone())
            {
                if (!AppMotion_StartMoveTo(params->bottom_pos, params->down_speed_pps))
                {
                    App_FailMotion();
                    return;
                }
                s_auto_phase = APP_AUTO_MOVE_DOWN;
                s_auto_phase_start_ms = HAL_GetTick();
            }
            else
            {
                App_FailMotion();
            }
            break;

        case APP_AUTO_MOVE_DOWN:
            if (AppMotion_IsMoving())
            {
                break;
            }
            if (AppMotion_TakeDone())
            {
                s_auto_wait_until_ms = HAL_GetTick() + params->delay_ms;
                s_auto_phase = APP_AUTO_DELAY;
            }
            else
            {
                App_FailMotion();
            }
            break;

        case APP_AUTO_DELAY:
            if ((int32_t)(HAL_GetTick() - s_auto_wait_until_ms) >= 0)
            {
                if (!AppMotion_StartMoveTo(params->top_pos, params->up_speed_pps))
                {
                    App_FailMotion();
                    return;
                }
                s_auto_phase = APP_AUTO_MOVE_UP;
                s_auto_phase_start_ms = HAL_GetTick();
            }
            break;

        case APP_AUTO_MOVE_UP:
            if (AppMotion_IsMoving())
            {
                break;
            }
            if (AppMotion_TakeDone())
            {
                s_state = APP_STATE_READY;
            }
            else
            {
                App_FailMotion();
            }
            break;

        default:
            App_FailMotion();
            break;
    }
}

static void App_SelectParam(int8_t delta)
{
    int16_t next = (int16_t)s_selected_param + delta;

    if (next < 0)
    {
        next = APP_PARAM_COUNT - 1;
    }
    if (next >= APP_PARAM_COUNT)
    {
        next = 0;
    }
    s_selected_param = (uint8_t)next;
}

static void App_StartEdit(void)
{
    if (s_state != APP_STATE_READY || s_edit_active)
    {
        return;
    }

    s_edit_params = *AppParams_Get();
    s_edit_index = s_selected_param;
    s_edit_active = true;
    s_save_pending = false;
    s_edit_error = false;
    s_edit_move_failed = false;
    s_edit_move_target_valid = false;
    s_edit_repeat_direction = 0;
}

static void App_ApplyEditPosition(void)
{
    int32_t target;

    if (!s_homed || !AppParams_IsValid(&s_edit_params))
    {
        AppMotion_Stop();
        s_edit_move_target_valid = false;
        s_edit_move_failed = true;
        s_edit_error = true;
        return;
    }

    target = (s_edit_index == APP_PARAM_BOTTOM)
                 ? s_edit_params.bottom_pos
                 : s_edit_params.top_pos;

    if (!AppMotion_SetTarget(target, APP_EDIT_SPEED_PPS))
    {
        s_edit_move_target_valid = false;
        s_edit_move_failed = true;
        s_edit_error = true;
        return;
    }

    s_edit_move_target = target;
    s_edit_move_target_valid = true;
    s_edit_move_failed = false;
    s_edit_error = false;
}

static void App_AdjustEdit(int8_t direction)
{
    int32_t delta = (direction > 0) ? 1 : -1;

    if (!s_edit_active || s_save_pending)
    {
        return;
    }

    s_edit_error = false;

    switch (s_edit_index)
    {
        case APP_PARAM_DELAY:
        {
            int32_t value = (int32_t)s_edit_params.delay_ms +
                            (delta * (int32_t)APP_EDIT_DELAY_STEP_MS);
            if (value < 0)
            {
                value = 0;
            }
            if (value > (int32_t)APP_PARAMS_MAX_DELAY_MS)
            {
                value = (int32_t)APP_PARAMS_MAX_DELAY_MS;
            }
            s_edit_params.delay_ms = (uint16_t)value;
            break;
        }

        case APP_PARAM_UP_SPEED:
        {
            int32_t value = (int32_t)s_edit_params.up_speed_pps +
                            (delta * (int32_t)APP_EDIT_SPEED_STEP_PPS);
            if (value < (int32_t)APP_PARAMS_MIN_SPEED_PPS)
            {
                value = (int32_t)APP_PARAMS_MIN_SPEED_PPS;
            }
            if (value > (int32_t)APP_PARAMS_MAX_SPEED_PPS)
            {
                value = (int32_t)APP_PARAMS_MAX_SPEED_PPS;
            }
            s_edit_params.up_speed_pps = (uint16_t)value;
            break;
        }

        case APP_PARAM_DOWN_SPEED:
        {
            int32_t value = (int32_t)s_edit_params.down_speed_pps +
                            (delta * (int32_t)APP_EDIT_SPEED_STEP_PPS);
            if (value < (int32_t)APP_PARAMS_MIN_SPEED_PPS)
            {
                value = (int32_t)APP_PARAMS_MIN_SPEED_PPS;
            }
            if (value > (int32_t)APP_PARAMS_MAX_SPEED_PPS)
            {
                value = (int32_t)APP_PARAMS_MAX_SPEED_PPS;
            }
            s_edit_params.down_speed_pps = (uint16_t)value;
            break;
        }

        case APP_PARAM_BOTTOM:
        {
            int64_t value = (int64_t)s_edit_params.bottom_pos +
                            ((int64_t)delta * APP_EDIT_POS_STEP_PULSES);
            int64_t max_bottom = (int64_t)s_edit_params.top_pos - 1;
            if (value > max_bottom)
            {
                value = max_bottom;
            }
            s_edit_params.bottom_pos = App_ClampI32(value);
            App_ApplyEditPosition();
            break;
        }

        case APP_PARAM_TOP:
        {
            int64_t value = (int64_t)s_edit_params.top_pos +
                            ((int64_t)delta * APP_EDIT_POS_STEP_PULSES);
            int64_t min_top = (int64_t)s_edit_params.bottom_pos + 1;
            if (value < min_top)
            {
                value = min_top;
            }
            if (value > -((int64_t)APP_PARAMS_TOP_CLEARANCE_PULSES))
            {
                value = -((int64_t)APP_PARAMS_TOP_CLEARANCE_PULSES);
            }
            s_edit_params.top_pos = App_ClampI32(value);
            App_ApplyEditPosition();
            break;
        }

        default:
            break;
    }
}

static void App_RequestSave(void)
{
    if (!s_edit_active || s_save_pending)
    {
        return;
    }

    s_edit_repeat_direction = 0;
    if (s_edit_move_failed || !AppParams_IsValid(&s_edit_params))
    {
        s_edit_error = true;
        return;
    }

    if (s_edit_index == APP_PARAM_BOTTOM || s_edit_index == APP_PARAM_TOP)
    {
        if (AppMotion_IsMoving())
        {
            s_save_pending = true;
            return;
        }
        if (s_edit_move_target_valid &&
            AppMotion_GetPosition() != s_edit_move_target)
        {
            s_edit_error = true;
            return;
        }
    }

    if (AppParams_Save(&s_edit_params))
    {
        App_FinishEdit();
    }
    else
    {
        s_edit_error = true;
    }
}

static void App_UpdateEditRepeat(uint32_t now)
{
    if (!s_edit_active || s_save_pending || s_edit_repeat_direction == 0)
    {
        return;
    }

    if (s_up.stable == s_down.stable)
    {
        s_edit_repeat_direction = 0;
        return;
    }
    if ((s_edit_repeat_direction > 0 && !s_up.stable) ||
        (s_edit_repeat_direction < 0 && !s_down.stable))
    {
        s_edit_repeat_direction = 0;
        return;
    }

    if ((uint32_t)(now - s_edit_repeat_start_ms) < APP_EDIT_REPEAT_DELAY_MS)
    {
        return;
    }

    if (!s_edit_repeat_started ||
        (uint32_t)(now - s_edit_repeat_last_ms) >= APP_EDIT_REPEAT_PERIOD_MS)
    {
        App_AdjustEdit(s_edit_repeat_direction);
        s_edit_repeat_started = true;
        s_edit_repeat_last_ms = now;
    }
}

static void App_ProcessEdit(uint32_t now)
{
    if (!s_edit_active)
    {
        return;
    }

    if (s_save_pending)
    {
        if (AppMotion_IsMoving())
        {
            return;
        }

        if (s_edit_move_failed || !s_edit_move_target_valid ||
            AppMotion_GetPosition() != s_edit_move_target)
        {
            s_save_pending = false;
            s_edit_error = true;
            return;
        }

        if (AppParams_Save(&s_edit_params))
        {
            App_FinishEdit();
        }
        else
        {
            s_save_pending = false;
            s_edit_error = true;
        }
        return;
    }

    App_UpdateEditRepeat(now);
}

static void App_HandleLongTrig(void)
{
    if (s_state == APP_STATE_MANUAL)
    {
        App_StartHoming();
    }
    else if (s_state == APP_STATE_READY && !s_edit_active)
    {
        App_StartAuto();
    }
}

static void App_HandleShortTrig(void)
{
    if (s_state != APP_STATE_READY || s_up.stable || s_down.stable)
    {
        return;
    }

    if (s_edit_active)
    {
        App_RequestSave();
    }
    else
    {
        App_StartEdit();
    }
}

static void App_UpdateTrig(uint32_t now)
{
    bool pressed = s_trig.stable;

    if (pressed && !s_trig_was_pressed)
    {
        s_trig_press_ms = s_trig.press_edge_ms;
        s_trig_press_state = s_state;
        s_trig_press_edit = s_edit_active;
        s_trig_long_fired = false;
    }

    if (pressed && !s_trig_long_fired &&
        (uint32_t)(now - s_trig_press_ms) >= APP_TRIG_LONG_PRESS_MS)
    {
        s_trig_long_fired = true;
        if (App_TrigPressMatches())
        {
            App_HandleLongTrig();
        }
    }

    if (!pressed && s_trig_was_pressed)
    {
        if (!s_trig_long_fired && App_TrigPressMatches())
        {
            App_HandleShortTrig();
        }
        s_trig_press_ms = 0U;
    }

    s_trig_was_pressed = pressed;
}

static void App_StartManualJog(app_motion_dir_t direction)
{
    if (s_state != APP_STATE_MANUAL || s_up.stable == s_down.stable)
    {
        return;
    }

    if (!AppMotion_StartJog(direction, APP_HOME_SPEED_PPS))
    {
        App_FailMotion();
    }
}

static void App_OnUpDownPressed(app_motion_dir_t direction, uint32_t now)
{
    if (!s_edit_active || s_save_pending || s_up.stable == s_down.stable)
    {
        return;
    }

    App_AdjustEdit((direction == APP_MOTION_DIR_UP) ? 1 : -1);
    s_edit_repeat_direction = (direction == APP_MOTION_DIR_UP) ? 1 : -1;
    s_edit_repeat_start_ms = now;
    s_edit_repeat_last_ms = now;
    s_edit_repeat_started = false;
}

static void App_OnUpDownReleased(app_motion_dir_t direction)
{
    bool press_edit = (direction == APP_MOTION_DIR_UP)
                          ? s_up_press_edit
                          : s_down_press_edit;
    app_state_t press_state = (direction == APP_MOTION_DIR_UP)
                                  ? s_up_press_state
                                  : s_down_press_state;
    bool long_fired = (direction == APP_MOTION_DIR_UP)
                          ? s_up_long_fired
                          : s_down_long_fired;

    if (s_edit_active)
    {
        if ((direction == APP_MOTION_DIR_UP && s_edit_repeat_direction > 0) ||
            (direction == APP_MOTION_DIR_DOWN && s_edit_repeat_direction < 0))
        {
            s_edit_repeat_direction = 0;
        }
        return;
    }

    if (s_state == APP_STATE_READY && press_state == APP_STATE_READY &&
        !press_edit && !long_fired)
    {
        App_SelectParam((direction == APP_MOTION_DIR_UP) ? -1 : 1);
    }
    else if (s_state == APP_STATE_MANUAL && press_state == APP_STATE_MANUAL &&
             AppMotion_IsJogging())
    {
        AppMotion_Stop();
    }
}

static void App_UpdateUpDown(uint32_t now)
{
    if (s_up.stable && !s_up_was_pressed)
    {
        s_up_press_ms = (s_up.press_edge_ms != 0U) ? s_up.press_edge_ms : now;
        s_up_press_state = s_state;
        s_up_press_edit = s_edit_active;
        s_up_long_fired = false;
        App_OnUpDownPressed(APP_MOTION_DIR_UP, s_up_press_ms);
    }
    if (s_down.stable && !s_down_was_pressed)
    {
        s_down_press_ms = (s_down.press_edge_ms != 0U) ? s_down.press_edge_ms : now;
        s_down_press_state = s_state;
        s_down_press_edit = s_edit_active;
        s_down_long_fired = false;
        App_OnUpDownPressed(APP_MOTION_DIR_DOWN, s_down_press_ms);
    }

    if (s_up.stable && !s_up_long_fired &&
        (uint32_t)(now - s_up_press_ms) >= APP_JOG_LONG_PRESS_MS)
    {
        s_up_long_fired = true;
        if (App_UpPressMatches() && s_state == APP_STATE_MANUAL && !s_down.stable)
        {
            App_StartManualJog(APP_MOTION_DIR_UP);
        }
    }
    if (s_down.stable && !s_down_long_fired &&
        (uint32_t)(now - s_down_press_ms) >= APP_JOG_LONG_PRESS_MS)
    {
        s_down_long_fired = true;
        if (App_DownPressMatches() && s_state == APP_STATE_MANUAL && !s_up.stable)
        {
            App_StartManualJog(APP_MOTION_DIR_DOWN);
        }
    }

    if (!s_up.stable && s_up_was_pressed)
    {
        App_OnUpDownReleased(APP_MOTION_DIR_UP);
    }
    if (!s_down.stable && s_down_was_pressed)
    {
        App_OnUpDownReleased(APP_MOTION_DIR_DOWN);
    }

    s_up_was_pressed = s_up.stable;
    s_down_was_pressed = s_down.stable;

    if (s_state == APP_STATE_MANUAL && s_up.stable && s_down.stable &&
        AppMotion_IsJogging())
    {
        AppMotion_Stop();
    }
}

void App_Init(void)
{
    uint32_t now;

    AppParams_Init();
    AppMotion_Init();

    now = HAL_GetTick();
    App_DebounceInit(&s_up, App_ReadUp(), now);
    App_DebounceInit(&s_down, App_ReadDown(), now);
    App_DebounceInit(&s_trig, App_ReadTrig(), now);
    App_DebounceInit(&s_origin, AppMotion_IsOriginActive(), now);

    s_state = APP_STATE_MANUAL;
    s_homed = false;
    s_homing_fault = false;
    s_selected_param = APP_PARAM_DELAY;
    s_edit_active = false;
    s_edit_index = APP_PARAM_DELAY;
    s_save_pending = false;
    s_edit_error = false;
    s_edit_move_failed = false;
    s_edit_move_target_valid = false;
    s_edit_repeat_direction = 0;

    s_trig_was_pressed = s_trig.stable;
    s_trig_long_fired = false;
    s_trig_press_ms = now;
    s_trig_press_state = s_state;
    s_trig_press_edit = false;

    s_up_was_pressed = s_up.stable;
    s_down_was_pressed = s_down.stable;
    s_up_long_fired = false;
    s_down_long_fired = false;
    s_up_press_ms = (s_up.press_edge_ms != 0U) ? s_up.press_edge_ms : now;
    s_down_press_ms = (s_down.press_edge_ms != 0U) ? s_down.press_edge_ms : now;
    s_up_press_state = s_state;
    s_down_press_state = s_state;
    s_up_press_edit = false;
    s_down_press_edit = false;
}

void App_Process(void)
{
    uint32_t now = HAL_GetTick();

    App_UpdateInputs(now);
    App_UpdateUpDown(now);
    App_UpdateTrig(now);

    if (s_state == APP_STATE_HOMING)
    {
        App_ProcessHoming(now);
    }
    else if (s_state == APP_STATE_AUTO)
    {
        App_ProcessAuto();
    }
    else if (s_state == APP_STATE_READY && s_edit_active)
    {
        App_ProcessEdit(now);
    }
}
