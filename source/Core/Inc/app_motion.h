#ifndef APP_MOTION_H
#define APP_MOTION_H

#include <stdbool.h>
#include <stdint.h>

#define APP_MOTION_TIMER_CLOCK_HZ 72000000UL
#define APP_MOTION_TIMER_ARR      99UL
#define APP_MOTION_START_PSC      1777UL

#define APP_MACHINE_PHYSICAL_TOP_POS     100      /* placeholder */
#define APP_MACHINE_PHYSICAL_BOTTOM_POS  (-9900)  /* placeholder */
#define APP_MACHINE_TRAVEL_PULSES \
    (APP_MACHINE_PHYSICAL_TOP_POS - APP_MACHINE_PHYSICAL_BOTTOM_POS) /* placeholder */

typedef enum
{
    APP_MOTION_DIR_DOWN = 0,
    APP_MOTION_DIR_UP = 1
} app_motion_dir_t;

void AppMotion_Init(void);
bool AppMotion_StartJog(app_motion_dir_t direction, uint16_t speed_pps);
bool AppMotion_StartMoveTo(int32_t target_pos, uint16_t speed_pps);
bool AppMotion_SetTarget(int32_t target_pos, uint16_t speed_pps);
bool AppMotion_StartRelative(app_motion_dir_t direction, uint32_t steps, uint16_t speed_pps);
void AppMotion_Stop(void);
void AppMotion_OnPulse(void);
bool AppMotion_IsMoving(void);
bool AppMotion_IsJogging(void);
bool AppMotion_TakeDone(void);
int32_t AppMotion_GetPosition(void);
void AppMotion_SetPosition(int32_t position);
uint32_t AppMotion_GetPulseCount(void);
app_motion_dir_t AppMotion_GetDirection(void);

#endif
