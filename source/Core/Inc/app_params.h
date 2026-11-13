#ifndef APP_PARAMS_H
#define APP_PARAMS_H

#include <stdbool.h>
#include <stdint.h>

#define APP_PARAMS_DEFAULT_DELAY_MS       300U    /* placeholder */
#define APP_PARAMS_DEFAULT_SPEED_PPS      23225U  /* placeholder */
#define APP_PARAMS_MIN_SPEED_PPS          12U
#define APP_PARAMS_MAX_SPEED_PPS          23225U
#define APP_PARAMS_MAX_DELAY_MS           60000U  /* placeholder */
#define APP_PARAMS_FACTORY_BOTTOM_POS     (-1000)  /* placeholder */
#define APP_PARAMS_FACTORY_TOP_POS        (-100)   /* placeholder */
#define APP_PARAMS_TOP_CLEARANCE_PULSES   100U   /* placeholder */

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

#endif
