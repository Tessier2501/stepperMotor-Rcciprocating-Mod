#ifndef APP_CORE_H
#define APP_CORE_H

#include <stdbool.h>
#include <stdint.h>

#include "app_params.h"
#include "app_view.h"

typedef enum
{
    APP_STATE_MANUAL = 0,
    APP_STATE_HOMING,
    APP_STATE_READY,
    APP_STATE_AUTO
} app_state_t;

typedef struct
{
    bool up_pressed;
    bool down_pressed;
    bool trig_pressed;
    bool origin_active;
} app_input_t;

typedef struct
{
    app_state_t state;
    bool homed;
    bool edit_active;
    app_param_index_t selected_param;
    int32_t position;
    bool moving;
    bool jogging;
} app_core_status_t;

void AppCore_Init(uint32_t now_ms, const app_input_t *input);
void AppCore_Process(uint32_t now_ms, const app_input_t *input);
void AppCore_OnPulse(void);
void AppCore_GetView(app_view_state_t *view);
void AppCore_GetStatus(app_core_status_t *status);

#endif
