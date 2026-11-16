#ifndef APP_PARAMS_H
#define APP_PARAMS_H

#include <stdbool.h>
#include <stdint.h>

#define APP_PARAMS_DEFAULT_DELAY_MS       300U
#define APP_PARAMS_DEFAULT_SPEED_PPS      405U
#define APP_PARAMS_MIN_SPEED_PPS          12U
#define APP_PARAMS_MAX_SPEED_PPS          23225U
#define APP_PARAMS_MAX_DELAY_MS           60000U
#define APP_PARAMS_FACTORY_BOTTOM_POS     (-9900)
#define APP_PARAMS_FACTORY_TOP_POS        (-100)
#define APP_PARAMS_MIN_BOTTOM_POS         (-9900)
#define APP_PARAMS_TOP_CLEARANCE_PULSES   100U

typedef enum
{
    APP_PARAM_DOWN_SPEED = 0,
    APP_PARAM_UP_SPEED,
    APP_PARAM_BOTTOM,
    APP_PARAM_TOP,
    APP_PARAM_DELAY,
    APP_PARAM_COUNT
} app_param_index_t;

typedef struct
{
    uint16_t delay_ms;
    uint16_t up_speed_pps;
    uint16_t down_speed_pps;
    int32_t bottom_pos;
    int32_t top_pos;
    uint32_t valid;
} app_params_t;

void AppParams_Init(void);
const app_params_t *AppParams_Get(void);
bool AppParams_IsUsable(void);
bool AppParams_IsValid(const app_params_t *params);
bool AppParams_Save(const app_params_t *params);
int32_t AppParams_GetValue(const app_params_t *params, app_param_index_t index);
const char *AppParams_Label(app_param_index_t index);
const char *AppParams_Unit(app_param_index_t index);

#endif
