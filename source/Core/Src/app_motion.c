#include "app_motion.h"
#include "app_platform.h"

#define MOTION_TIMER_CLOCK_HZ APP_MOTION_TIMER_CLOCK_HZ
#define MOTION_TIMER_ARR      APP_MOTION_TIMER_ARR
#define MOTION_START_PSC      APP_MOTION_START_PSC
#define MOTION_RAMP_POINTS    150U
#define MOTION_RAMP_MAX_STEPS 150U

static const uint32_t s_ramp_psc[MOTION_RAMP_POINTS] = {1777, 1765, 1754, 1742, 1730, 1718, 1707, 1695, 1683, 1671, 1660, 1648, 1636, 1625, 1613, 1601, 1589, 1578, 1566, 1554, 1543, 1531, 1519, 1507, 1496, 1484, 1472, 1460, 1449, 1437, 1425, 1414, 1402, 1390, 1378, 1367, 1355, 1343, 1331, 1320, 1308, 1296, 1285, 1273, 1261, 1249, 1238, 1226, 1214, 1202, 1191, 1179, 1167, 1156, 1144, 1132, 1120, 1109, 1097, 1085, 1074, 1062, 1050, 1038, 1027, 1015, 1003, 991, 980, 968, 956, 945, 933, 921, 909, 898, 886, 874, 862, 851, 839, 827, 816, 804, 792, 780, 769, 757, 745, 733, 722, 710, 698, 687, 675, 663, 651, 640, 628, 616, 605, 593, 581, 569, 558, 546, 534, 522, 511, 499, 487, 476, 464, 452, 440, 429, 417, 405, 393, 382, 370, 358, 347, 335, 323, 311, 300, 288, 276, 265, 253, 241, 229, 218, 206, 194, 182, 171, 159, 147, 136, 124, 112, 100, 89, 77, 65, 53, 42, 30};

static volatile bool s_moving;
static volatile bool s_jogging;
static volatile bool s_done;
static volatile int32_t s_position;
static volatile uint32_t s_pulse_count;
static volatile uint32_t s_remaining_steps;
static volatile uint32_t s_ramp_steps;
static volatile uint32_t s_psc_cruise;
static volatile app_motion_dir_t s_direction;

static uint32_t AppMotion_PscFromPps(uint16_t speed_pps)
{
    uint32_t denominator;
    uint32_t psc;

    if (speed_pps == 0U)
    {
        return 65535U;
    }

    denominator = (uint32_t)speed_pps * (MOTION_TIMER_ARR + 1U);
    psc = MOTION_TIMER_CLOCK_HZ / denominator;
    if (psc == 0U)
    {
        return 0U;
    }
    psc--;
    return (psc > 65535U) ? 65535U : psc;
}

static uint32_t AppMotion_RampPsc(uint32_t point)
{
    int32_t start = (int32_t)MOTION_START_PSC;
    int32_t target = (int32_t)s_psc_cruise;
    int32_t span;
    int32_t delta;
    int32_t value;

    if (point >= MOTION_RAMP_POINTS)
    {
        point = MOTION_RAMP_POINTS - 1U;
    }

    span = (int32_t)s_ramp_psc[MOTION_RAMP_POINTS - 1U] - (int32_t)s_ramp_psc[0];
    delta = (int32_t)s_ramp_psc[point] - (int32_t)s_ramp_psc[0];

    if (span == 0)
    {
        return (uint32_t)start;
    }

    value = start + ((delta * (target - start)) / span);
    if (value < 0)
    {
        value = 0;
    }
    if (value > 65535)
    {
        value = 65535;
    }
    return (uint32_t)value;
}

static void AppMotion_UpdatePsc(void)
{
    uint32_t point;

    if (s_jogging || s_ramp_steps < 2U)
    {
        PlatformMotion_SetPsc(s_psc_cruise);
        return;
    }

    if (s_pulse_count < s_ramp_steps)
    {
        point = ((s_pulse_count - 1U) * (MOTION_RAMP_POINTS - 1U)) /
                (s_ramp_steps - 1U);
    }
    else if (s_remaining_steps < s_ramp_steps)
    {
        uint32_t remaining = (s_remaining_steps > 0U) ? (s_remaining_steps - 1U) : 0U;
        point = (remaining * (MOTION_RAMP_POINTS - 1U)) / (s_ramp_steps - 1U);
    }
    else
    {
        point = MOTION_RAMP_POINTS - 1U;
    }

    PlatformMotion_SetPsc(AppMotion_RampPsc(point));
}

static void AppMotion_Finish(void)
{
    s_moving = false;
    s_jogging = false;
    PlatformMotion_Stop();
    s_done = true;
}

static bool AppMotion_Start(app_motion_dir_t direction,
                            uint32_t steps,
                            uint16_t speed_pps,
                            bool jogging,
                            bool use_ramp)
{
    uint32_t initial_psc;

    AppMotion_Stop();

    s_direction = direction;
    s_jogging = jogging;
    s_remaining_steps = steps;
    s_pulse_count = 0U;
    s_done = false;
    s_psc_cruise = AppMotion_PscFromPps(speed_pps);

    if (jogging || steps == 0U || !use_ramp)
    {
        s_ramp_steps = 0U;
    }
    else
    {
        s_ramp_steps = steps / 2U;
        if (s_ramp_steps > MOTION_RAMP_MAX_STEPS)
        {
            s_ramp_steps = MOTION_RAMP_MAX_STEPS;
        }
        if (s_psc_cruise >= MOTION_START_PSC)
        {
            s_ramp_steps = 0U;
        }
    }

    if (!jogging && steps == 0U)
    {
        s_jogging = false;
        s_done = true;
        return true;
    }

    initial_psc = (s_ramp_steps > 0U) ? MOTION_START_PSC : s_psc_cruise;
    s_moving = true;

    if (!PlatformMotion_Start(direction, initial_psc))
    {
        s_moving = false;
        s_jogging = false;
        PlatformMotion_Stop();
        return false;
    }

    return true;
}

static bool AppMotion_StartFixedMoveTo(int32_t target_pos, uint16_t speed_pps)
{
    int64_t delta;
    app_motion_dir_t direction;

    if (s_moving)
    {
        AppMotion_Stop();
    }

    delta = (int64_t)target_pos - (int64_t)s_position;
    if (delta == 0)
    {
        s_done = true;
        return true;
    }
    if (delta > 0)
    {
        direction = APP_MOTION_DIR_UP;
    }
    else
    {
        direction = APP_MOTION_DIR_DOWN;
        delta = -delta;
    }
    if (delta > UINT32_MAX)
    {
        return false;
    }
    return AppMotion_Start(direction, (uint32_t)delta, speed_pps, false, false);
}

void AppMotion_Init(void)
{
    s_moving = false;
    s_jogging = false;
    s_done = false;
    s_position = 0;
    s_pulse_count = 0U;
    s_remaining_steps = 0U;
    s_ramp_steps = 0U;
    s_psc_cruise = MOTION_START_PSC;
    s_direction = APP_MOTION_DIR_DOWN;
    PlatformMotion_Stop();
}

bool AppMotion_StartJog(app_motion_dir_t direction, uint16_t speed_pps)
{
    return AppMotion_Start(direction, 0U, speed_pps, true, false);
}

bool AppMotion_StartMoveTo(int32_t target_pos, uint16_t speed_pps)
{
    int64_t delta;
    app_motion_dir_t direction;

    if (s_moving)
    {
        AppMotion_Stop();
    }

    delta = (int64_t)target_pos - (int64_t)s_position;
    if (delta == 0)
    {
        AppMotion_Stop();
        s_done = true;
        return true;
    }
    if (delta > 0)
    {
        direction = APP_MOTION_DIR_UP;
    }
    else
    {
        direction = APP_MOTION_DIR_DOWN;
        delta = -delta;
    }
    if (delta > UINT32_MAX)
    {
        return false;
    }
    return AppMotion_Start(direction, (uint32_t)delta, speed_pps, false, true);
}

bool AppMotion_StartRelative(app_motion_dir_t direction, uint32_t steps, uint16_t speed_pps)
{
    return AppMotion_Start(direction, steps, speed_pps, false, true);
}

bool AppMotion_SetTarget(int32_t target_pos, uint16_t speed_pps)
{
    int64_t delta;
    app_motion_dir_t direction;
    uint32_t irq_state;
    bool moving;
    bool jogging;

    irq_state = PlatformIrqSave();
    moving = s_moving;
    jogging = s_jogging;

    if (!moving || jogging)
    {
        PlatformIrqRestore(irq_state);
        if (jogging)
        {
            AppMotion_Stop();
        }
        return AppMotion_StartFixedMoveTo(target_pos, speed_pps);
    }

    delta = (int64_t)target_pos - (int64_t)s_position;
    if (delta == 0)
    {
        PlatformIrqRestore(irq_state);
        AppMotion_Stop();
        s_done = true;
        return true;
    }

    direction = (delta > 0) ? APP_MOTION_DIR_UP : APP_MOTION_DIR_DOWN;
    if (direction != s_direction)
    {
        PlatformIrqRestore(irq_state);
        AppMotion_Stop();
        return AppMotion_StartFixedMoveTo(target_pos, speed_pps);
    }

    if (delta < 0)
    {
        delta = -delta;
    }
    if (delta > UINT32_MAX)
    {
        PlatformIrqRestore(irq_state);
        return false;
    }

    s_remaining_steps = (uint32_t)delta;
    s_ramp_steps = 0U;
    s_psc_cruise = AppMotion_PscFromPps(speed_pps);
    s_done = false;
    PlatformMotion_SetPsc(s_psc_cruise);
    PlatformIrqRestore(irq_state);
    return true;
}

void AppMotion_Stop(void)
{
    uint32_t irq_state = PlatformIrqSave();

    s_moving = false;
    s_jogging = false;
    s_done = false;
    s_ramp_steps = 0U;
    PlatformMotion_Stop();
    PlatformIrqRestore(irq_state);
}

bool AppMotion_IsMoving(void)
{
    return s_moving;
}

bool AppMotion_IsJogging(void)
{
    return s_moving && s_jogging;
}

bool AppMotion_TakeDone(void)
{
    bool done = s_done;
    s_done = false;
    return done;
}

int32_t AppMotion_GetPosition(void)
{
    return s_position;
}

void AppMotion_SetPosition(int32_t position)
{
    uint32_t irq_state = PlatformIrqSave();
    s_position = position;
    PlatformIrqRestore(irq_state);
}

uint32_t AppMotion_GetPulseCount(void)
{
    return s_pulse_count;
}

app_motion_dir_t AppMotion_GetDirection(void)
{
    return s_direction;
}

void AppMotion_OnPulse(void)
{
    if (!s_moving)
    {
        return;
    }

    s_pulse_count++;
    s_position += (s_direction == APP_MOTION_DIR_UP) ? 1 : -1;

    if (!s_jogging)
    {
        if (s_remaining_steps > 0U)
        {
            s_remaining_steps--;
        }
        if (s_remaining_steps == 0U)
        {
            AppMotion_Finish();
            return;
        }
    }

    AppMotion_UpdatePsc();
}
