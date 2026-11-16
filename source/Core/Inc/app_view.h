#ifndef APP_VIEW_H
#define APP_VIEW_H

#include <stdint.h>

#include "app_params.h"

#define APP_VIEW_WIDTH  80U
#define APP_VIEW_HEIGHT 160U

typedef enum
{
    APP_VIEW_SCREEN_UNHOMED = 0,
    APP_VIEW_SCREEN_MAIN,
    APP_VIEW_SCREEN_EDIT
} app_view_screen_t;

typedef struct
{
    app_view_screen_t screen;
    app_param_index_t selected_param;
    app_param_index_t edit_param;
    app_params_t params;
    app_params_t edit_old;
    app_params_t edit_new;
} app_view_state_t;

void AppView_Render(const app_view_state_t *state);

#endif
