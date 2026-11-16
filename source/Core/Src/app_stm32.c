#include "app.h"
#include "app_core.h"
#include "app_platform.h"
#include "bsp_st7735.h"
#include "main.h"
#include "tim.h"

#include <string.h>

#define PARAMS_FLASH_PAGE_ADDR 0x0800FC00UL
#define PARAMS_FLASH_ADDR      (PARAMS_FLASH_PAGE_ADDR + 0x100UL)

static app_input_t AppStm32_ReadInputs(void)
{
    app_input_t input;

    input.up_pressed = HAL_GPIO_ReadPin(Up_GPIO_Port, Up_Pin) == GPIO_PIN_RESET;
    input.down_pressed = HAL_GPIO_ReadPin(Down_GPIO_Port, Down_Pin) == GPIO_PIN_RESET;
    input.trig_pressed = HAL_GPIO_ReadPin(Trig_GPIO_Port, Trig_Pin) == GPIO_PIN_RESET;
    input.origin_active = HAL_GPIO_ReadPin(Origin_GPIO_Port, Origin_Pin) == GPIO_PIN_RESET;
    return input;
}

bool Platform_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Pin = Origin_Pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(Origin_GPIO_Port, &gpio);

    HAL_GPIO_WritePin(En_GPIO_Port, En_Pin, GPIO_PIN_RESET);
    HAL_TIM_PWM_Stop_IT(&htim1, TIM_CHANNEL_1);

    return BspDisplay_Init();
}

void App_Init(void)
{
    app_input_t input;

    if (!Platform_Init())
    {
        Error_Handler();
    }
    input = AppStm32_ReadInputs();
    AppCore_Init(HAL_GetTick(), &input);
}

void App_Process(void)
{
    app_input_t input = AppStm32_ReadInputs();
    AppCore_Process(HAL_GetTick(), &input);
}

bool PlatformMotion_Start(app_motion_dir_t direction, uint32_t initial_psc)
{
    HAL_GPIO_WritePin(Dir_GPIO_Port, Dir_Pin,
                      (direction == APP_MOTION_DIR_UP) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(En_GPIO_Port, En_Pin, GPIO_PIN_SET);

    htim1.Instance->PSC = initial_psc;
    htim1.Instance->EGR = TIM_EGR_UG;
    __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_UPDATE);
    __HAL_TIM_SET_COUNTER(&htim1, 0U);
    __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_CC1);

    if (HAL_TIM_PWM_Start_IT(&htim1, TIM_CHANNEL_1) != HAL_OK)
    {
        HAL_GPIO_WritePin(En_GPIO_Port, En_Pin, GPIO_PIN_RESET);
        return false;
    }
    return true;
}

void PlatformMotion_SetPsc(uint32_t psc)
{
    htim1.Instance->PSC = psc;
}

void PlatformMotion_Stop(void)
{
    HAL_TIM_PWM_Stop_IT(&htim1, TIM_CHANNEL_1);
    HAL_GPIO_WritePin(En_GPIO_Port, En_Pin, GPIO_PIN_RESET);
}

bool PlatformStorage_Load(app_params_t *params)
{
    memcpy(params, (const void *)PARAMS_FLASH_ADDR, sizeof(*params));
    return true;
}

bool PlatformStorage_Save(const app_params_t *params)
{
    uint8_t bytes[sizeof(app_params_t)];
    uint32_t words = (uint32_t)(sizeof(bytes) / sizeof(uint32_t));
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t page_error = 0U;
    uint32_t irq_state;
    bool ok = true;

    memset(bytes, 0, sizeof(bytes));
    memcpy(bytes, params, sizeof(*params));

    irq_state = PlatformIrqSave();
    HAL_FLASH_Unlock();
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = PARAMS_FLASH_PAGE_ADDR;
    erase.NbPages = 1U;
    if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK)
    {
        ok = false;
    }

    for (uint32_t i = 0U; ok && i < words; i++)
    {
        uint32_t word = 0U;
        memcpy(&word, &bytes[i * sizeof(uint32_t)], sizeof(word));
        if (HAL_FLASH_Program(TYPEPROGRAM_WORD,
                              PARAMS_FLASH_ADDR + (i * sizeof(uint32_t)),
                              word) != HAL_OK)
        {
            ok = false;
        }
    }

    HAL_FLASH_Lock();
    PlatformIrqRestore(irq_state);

    if (ok && memcmp((const void *)PARAMS_FLASH_ADDR, bytes, sizeof(bytes)) != 0)
    {
        ok = false;
    }
    return ok;
}

uint32_t PlatformIrqSave(void)
{
    uint32_t state = __get_PRIMASK();
    __disable_irq();
    return state;
}

void PlatformIrqRestore(uint32_t state)
{
    if ((state & 1U) == 0U)
    {
        __enable_irq();
    }
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1)
    {
        AppCore_OnPulse();
    }
}
