#include "app_params.h"
#include "app_platform.h"

#include <stddef.h>
#include <string.h>

_Static_assert(sizeof(app_params_t) == 20U, "app_params_t size");
_Static_assert(offsetof(app_params_t, valid) == 16U, "valid marker must be last word");
_Static_assert(APP_PARAMS_FACTORY_BOTTOM_POS < APP_PARAMS_FACTORY_TOP_POS,
               "factory endpoints out of order");
_Static_assert(APP_PARAMS_FACTORY_BOTTOM_POS >= APP_PARAMS_MIN_BOTTOM_POS,
               "factory bottom outside physical range");
_Static_assert(APP_PARAMS_FACTORY_TOP_POS <= -((int32_t)APP_PARAMS_TOP_CLEARANCE_PULSES),
               "factory top clearance");

#define APP_PARAMS_VALID_MARKER 1UL

static app_params_t s_params;

static bool AppParams_FieldsValid(const app_params_t *params)
{
    if (params->delay_ms > APP_PARAMS_MAX_DELAY_MS)
    {
        return false;
    }
    if (params->up_speed_pps < APP_PARAMS_MIN_SPEED_PPS ||
        params->up_speed_pps > APP_PARAMS_MAX_SPEED_PPS ||
        params->down_speed_pps < APP_PARAMS_MIN_SPEED_PPS ||
        params->down_speed_pps > APP_PARAMS_MAX_SPEED_PPS)
    {
        return false;
    }
    if (params->top_pos > -((int32_t)APP_PARAMS_TOP_CLEARANCE_PULSES) ||
        params->bottom_pos < APP_PARAMS_MIN_BOTTOM_POS ||
        params->bottom_pos >= params->top_pos)
    {
        return false;
    }
    return true;
}

static void AppParams_SetDefaults(void)
{
    memset(&s_params, 0, sizeof(s_params));
    s_params.delay_ms = APP_PARAMS_DEFAULT_DELAY_MS;
    s_params.up_speed_pps = APP_PARAMS_DEFAULT_SPEED_PPS;
    s_params.down_speed_pps = APP_PARAMS_DEFAULT_SPEED_PPS;
    s_params.bottom_pos = APP_PARAMS_FACTORY_BOTTOM_POS;
    s_params.top_pos = APP_PARAMS_FACTORY_TOP_POS;
    s_params.valid = APP_PARAMS_VALID_MARKER;
}

void AppParams_Init(void)
{
    app_params_t stored;

    if (PlatformStorage_Load(&stored) &&
        stored.valid == APP_PARAMS_VALID_MARKER &&
        AppParams_FieldsValid(&stored))
    {
        s_params = stored;
    }
    else
    {
        AppParams_SetDefaults();
    }
}

const app_params_t *AppParams_Get(void)
{
    return &s_params;
}

bool AppParams_IsUsable(void)
{
    return s_params.valid == APP_PARAMS_VALID_MARKER && AppParams_IsValid(&s_params);
}

bool AppParams_IsValid(const app_params_t *params)
{
    return AppParams_FieldsValid(params);
}

bool AppParams_Save(const app_params_t *params)
{
    app_params_t saved;

    if (!AppParams_IsValid(params))
    {
        return false;
    }

    saved = *params;
    saved.valid = APP_PARAMS_VALID_MARKER;

    if (!PlatformStorage_Save(&saved))
    {
        return false;
    }

    s_params = saved;
    return true;
}

int32_t AppParams_GetValue(const app_params_t *params, app_param_index_t index)
{
    switch (index)
    {
        case APP_PARAM_DOWN_SPEED:
            return (int32_t)params->down_speed_pps;
        case APP_PARAM_UP_SPEED:
            return (int32_t)params->up_speed_pps;
        case APP_PARAM_BOTTOM:
            return params->bottom_pos;
        case APP_PARAM_TOP:
            return params->top_pos;
        case APP_PARAM_DELAY:
            return (int32_t)params->delay_ms;
        default:
            return 0;
    }
}

const char *AppParams_Label(app_param_index_t index)
{
    switch (index)
    {
        case APP_PARAM_DOWN_SPEED:
            return "下行速度";
        case APP_PARAM_UP_SPEED:
            return "上行速度";
        case APP_PARAM_BOTTOM:
            return "下止点";
        case APP_PARAM_TOP:
            return "上止点";
        case APP_PARAM_DELAY:
            return "延时";
        default:
            return "";
    }
}

const char *AppParams_Unit(app_param_index_t index)
{
    switch (index)
    {
        case APP_PARAM_DOWN_SPEED:
        case APP_PARAM_UP_SPEED:
            return "pps";
        case APP_PARAM_BOTTOM:
        case APP_PARAM_TOP:
            return "脉冲";
        case APP_PARAM_DELAY:
            return "ms";
        default:
            return "";
    }
}
