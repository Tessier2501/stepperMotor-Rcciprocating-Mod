#include "app_params.h"
#include "main.h"

#include <stddef.h>
#include <string.h>

_Static_assert(sizeof(app_params_t) == 20U, "app_params_t size");
_Static_assert(offsetof(app_params_t, valid) == 16U, "valid marker must be last word");
_Static_assert(APP_PARAMS_FACTORY_BOTTOM_POS < APP_PARAMS_FACTORY_TOP_POS, "factory endpoints out of order");
_Static_assert(APP_PARAMS_FACTORY_TOP_POS <= -((int32_t)APP_PARAMS_TOP_CLEARANCE_PULSES), "factory top clearance");

#define APP_PARAMS_FLASH_PAGE_ADDR 0x0800FC00UL
#define APP_PARAMS_FLASH_ADDR      (APP_PARAMS_FLASH_PAGE_ADDR + 0x100UL)
#define APP_PARAMS_VALID_MARKER    1UL

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

static bool AppParams_FlashWrite(const app_params_t *params)
{
    uint8_t bytes[sizeof(app_params_t)];
    uint32_t words = (uint32_t)(sizeof(bytes) / sizeof(uint32_t));
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t page_error = 0;
    uint32_t primask = __get_PRIMASK();
    bool ok = true;

    memset(bytes, 0, sizeof(bytes));
    memcpy(bytes, params, sizeof(*params));

    __disable_irq();
    HAL_FLASH_Unlock();

    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = APP_PARAMS_FLASH_PAGE_ADDR;
    erase.NbPages = 1;
    if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK)
    {
        ok = false;
    }

    if (ok)
    {
        for (uint32_t i = 0; i < words; i++)
        {
            uint32_t word = 0;
            memcpy(&word, &bytes[i * sizeof(uint32_t)], sizeof(word));
            if (i == (words - 1U))
            {
                break;
            }
            if (HAL_FLASH_Program(TYPEPROGRAM_WORD,
                                  APP_PARAMS_FLASH_ADDR + (i * sizeof(uint32_t)),
                                  word) != HAL_OK)
            {
                ok = false;
                break;
            }
        }
    }

    if (ok && words != 0U)
    {
        uint32_t word = 0;
        memcpy(&word, &bytes[(words - 1U) * sizeof(uint32_t)], sizeof(word));
        if (HAL_FLASH_Program(TYPEPROGRAM_WORD,
                              APP_PARAMS_FLASH_ADDR + ((words - 1U) * sizeof(uint32_t)),
                              word) != HAL_OK)
        {
            ok = false;
        }
    }

    HAL_FLASH_Lock();
    if ((primask & 1U) == 0U)
    {
        __enable_irq();
    }

    if (ok && memcmp((const void *)APP_PARAMS_FLASH_ADDR, bytes, sizeof(bytes)) != 0)
    {
        ok = false;
    }

    return ok;
}

void AppParams_Init(void)
{
    const app_params_t *stored = (const app_params_t *)APP_PARAMS_FLASH_ADDR;

    if (stored->valid == APP_PARAMS_VALID_MARKER && AppParams_FieldsValid(stored))
    {
        memcpy(&s_params, stored, sizeof(s_params));
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

    if (!AppParams_FlashWrite(&saved))
    {
        return false;
    }

    s_params = saved;
    return true;
}
