#include "bsp_st7735.h"
#include "app_platform.h"
#include "app_view.h"
#include "main.h"

#define LCD_SPI_INSTANCE            SPI2
#define LCD_GPIO_PORT               GPIOB
#define LCD_SCK_PIN                 GPIO_PIN_13
#define LCD_MOSI_PIN                GPIO_PIN_15
#define LCD_CS_PIN                  GPIO_PIN_12
#define LCD_DC_PIN                  GPIO_PIN_14
#define LCD_RESET_PIN               GPIO_PIN_1
#define LCD_BACKLIGHT_PIN           GPIO_PIN_10
#define LCD_BACKLIGHT_ACTIVE_HIGH   1
#define LCD_X_OFFSET                26U
#define LCD_Y_OFFSET                1U

static bool s_spi_ok;
static uint8_t s_pixel_buffer[128];

static uint16_t BspDisplay_Color(ui_color_t color)
{
    switch (color)
    {
        case UI_COLOR_WHITE:
            return 0xFFFFU;
        case UI_COLOR_RED:
            return 0xF800U;
        default:
            return 0x0000U;
    }
}

static void BspDisplay_Transmit(const uint8_t *data, uint16_t length)
{
    for (uint16_t i = 0U; i < length; i++)
    {
        while ((LCD_SPI_INSTANCE->SR & SPI_SR_TXE) == 0U)
        {
        }
        *(__IO uint8_t *)&LCD_SPI_INSTANCE->DR = data[i];
        while ((LCD_SPI_INSTANCE->SR & SPI_SR_RXNE) == 0U)
        {
        }
        (void)*(__IO uint8_t *)&LCD_SPI_INSTANCE->DR;
    }
    while ((LCD_SPI_INSTANCE->SR & SPI_SR_BSY) != 0U)
    {
    }
}

static bool BspDisplay_WriteCommand(uint8_t command)
{
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_CS_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_DC_PIN, GPIO_PIN_RESET);
    BspDisplay_Transmit(&command, 1U);
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_CS_PIN, GPIO_PIN_SET);
    return s_spi_ok;
}

static bool BspDisplay_WriteData(const uint8_t *data, uint16_t length)
{
    if (length == 0U)
    {
        return true;
    }
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_CS_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_DC_PIN, GPIO_PIN_SET);
    BspDisplay_Transmit(data, length);
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_CS_PIN, GPIO_PIN_SET);
    return s_spi_ok;
}

static void BspDisplay_CommandData(uint8_t command, const uint8_t *data,
                                   uint16_t length)
{
    if (!s_spi_ok)
    {
        return;
    }
    s_spi_ok = BspDisplay_WriteCommand(command);
    if (s_spi_ok)
    {
        s_spi_ok = BspDisplay_WriteData(data, length);
    }
}

static void BspDisplay_SetWindow(uint8_t x, uint8_t y, uint8_t width,
                                 uint8_t height)
{
    uint8_t data[4];
    uint16_t x0 = (uint16_t)x + LCD_X_OFFSET;
    uint16_t y0 = (uint16_t)y + LCD_Y_OFFSET;
    uint16_t x1 = (uint16_t)(x0 + width - 1U);
    uint16_t y1 = (uint16_t)(y0 + height - 1U);

    data[0] = (uint8_t)(x0 >> 8U);
    data[1] = (uint8_t)x0;
    data[2] = (uint8_t)(x1 >> 8U);
    data[3] = (uint8_t)x1;
    BspDisplay_CommandData(0x2AU, data, sizeof(data));

    data[0] = (uint8_t)(y0 >> 8U);
    data[1] = (uint8_t)y0;
    data[2] = (uint8_t)(y1 >> 8U);
    data[3] = (uint8_t)y1;
    BspDisplay_CommandData(0x2BU, data, sizeof(data));
    BspDisplay_CommandData(0x2CU, NULL, 0U);
}

static void BspDisplay_FlushPixels(uint16_t count)
{
    if (!s_spi_ok || count == 0U)
    {
        return;
    }
    BspDisplay_Transmit(s_pixel_buffer, (uint16_t)(count * 2U));
}

void PlatformDisplay_FillRect(int16_t x, int16_t y, uint8_t width,
                              uint8_t height, ui_color_t color)
{
    uint16_t pixel_color;
    uint16_t remaining;
    uint16_t index;
    int32_t clipped_x = x;
    int32_t clipped_y = y;
    int32_t clipped_w = width;
    int32_t clipped_h = height;

    if (!s_spi_ok || clipped_w <= 0 || clipped_h <= 0)
    {
        return;
    }
    if (clipped_x < 0)
    {
        clipped_w += clipped_x;
        clipped_x = 0;
    }
    if (clipped_y < 0)
    {
        clipped_h += clipped_y;
        clipped_y = 0;
    }
    if (clipped_x + clipped_w > (int32_t)APP_VIEW_WIDTH)
    {
        clipped_w = (int32_t)APP_VIEW_WIDTH - clipped_x;
    }
    if (clipped_y + clipped_h > (int32_t)APP_VIEW_HEIGHT)
    {
        clipped_h = (int32_t)APP_VIEW_HEIGHT - clipped_y;
    }
    if (clipped_w <= 0 || clipped_h <= 0)
    {
        return;
    }

    pixel_color = BspDisplay_Color(color);
    BspDisplay_SetWindow((uint8_t)clipped_x, (uint8_t)clipped_y,
                         (uint8_t)clipped_w, (uint8_t)clipped_h);
    if (!s_spi_ok)
    {
        return;
    }

    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_CS_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_DC_PIN, GPIO_PIN_SET);
    for (index = 0U; index < 64U; index++)
    {
        s_pixel_buffer[index * 2U] = (uint8_t)(pixel_color >> 8U);
        s_pixel_buffer[index * 2U + 1U] = (uint8_t)pixel_color;
    }
    remaining = (uint16_t)((uint32_t)clipped_w * (uint32_t)clipped_h);
    while (remaining > 0U && s_spi_ok)
    {
        uint16_t count = (remaining > 64U) ? 64U : remaining;
        BspDisplay_FlushPixels(count);
        remaining = (uint16_t)(remaining - count);
    }
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_CS_PIN, GPIO_PIN_SET);
}

void PlatformDisplay_DrawBitmap(int16_t x, int16_t y, uint8_t width,
                                uint8_t height, const uint8_t *bits,
                                ui_color_t foreground, ui_color_t background)
{
    uint16_t foreground_color;
    uint16_t background_color;
    uint8_t row_bytes;
    uint16_t index = 0U;
    uint8_t row;
    uint8_t column;

    if (!s_spi_ok || bits == NULL || width == 0U || height == 0U)
    {
        return;
    }

    foreground_color = BspDisplay_Color(foreground);
    background_color = BspDisplay_Color(background);
    row_bytes = (uint8_t)((width + 7U) / 8U);
    BspDisplay_SetWindow((uint8_t)x, (uint8_t)y, width, height);
    if (!s_spi_ok)
    {
        return;
    }

    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_CS_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_DC_PIN, GPIO_PIN_SET);
    for (row = 0U; row < height; row++)
    {
        for (column = 0U; column < width; column++)
        {
            bool set = (bits[row * row_bytes + (column >> 3U)] &
                        (uint8_t)(1U << (column & 7U))) != 0U;
            uint16_t color = set ? foreground_color : background_color;
            s_pixel_buffer[index * 2U] = (uint8_t)(color >> 8U);
            s_pixel_buffer[index * 2U + 1U] = (uint8_t)color;
            index++;
            if (index == 64U)
            {
                BspDisplay_FlushPixels(index);
                index = 0U;
            }
        }
    }
    if (index > 0U)
    {
        BspDisplay_FlushPixels(index);
    }
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_CS_PIN, GPIO_PIN_SET);
}

void PlatformDisplay_Present(void)
{
}

bool BspDisplay_Init(void)
{
    static const uint8_t normal_mode[] = {0x05U, 0x3CU, 0x3CU};
    static const uint8_t idle_mode[] = {0x05U, 0x3CU, 0x3CU};
    static const uint8_t partial_mode[] = {0x05U, 0x3CU, 0x3CU, 0x05U, 0x3CU, 0x3CU};
    static const uint8_t dot_inversion[] = {0x03U};
    static const uint8_t power[] = {0x04U, 0x04U, 0x84U};
    static const uint8_t vgh_vgl[] = {0x06U};
    static const uint8_t normal_power[] = {0x0DU, 0x00U};
    static const uint8_t idle_power[] = {0x8DU, 0x6AU};
    static const uint8_t partial_power[] = {0x8DU, 0xEEU};
    static const uint8_t vcom[] = {0x3AU};
    static const uint8_t gamma_positive[] = {
        0x10U, 0x0EU, 0x05U, 0x0DU, 0x0EU, 0x06U, 0x03U, 0x07U,
        0x0AU, 0x0EU, 0x1DU, 0x32U, 0x03U, 0x0BU, 0x00U, 0x2FU
    };
    static const uint8_t gamma_negative[] = {
        0x0EU, 0x12U, 0x05U, 0x07U, 0x0CU, 0x05U, 0x02U, 0x06U,
        0x07U, 0x0BU, 0x13U, 0x2CU, 0x03U, 0x0DU, 0x02U, 0x1FU
    };
    static const uint8_t color_mode[] = {0x05U};
    static const uint8_t madctl[] = {0xC8U};
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_SPI2_CLK_ENABLE();

    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_CS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_DC_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_RESET_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_BACKLIGHT_PIN,
                      (LCD_BACKLIGHT_ACTIVE_HIGH != 0) ? GPIO_PIN_RESET : GPIO_PIN_SET);

    gpio.Pin = LCD_CS_PIN | LCD_DC_PIN | LCD_RESET_PIN | LCD_BACKLIGHT_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(LCD_GPIO_PORT, &gpio);

    gpio.Pin = LCD_SCK_PIN | LCD_MOSI_PIN;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(LCD_GPIO_PORT, &gpio);

    LCD_SPI_INSTANCE->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI |
                            SPI_CR1_BR_0;
    LCD_SPI_INSTANCE->CR1 |= SPI_CR1_SPE;
    s_spi_ok = true;
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_RESET_PIN, GPIO_PIN_RESET);
    HAL_Delay(10U);
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_RESET_PIN, GPIO_PIN_SET);
    HAL_Delay(120U);

    BspDisplay_CommandData(0x11U, NULL, 0U);
    HAL_Delay(120U);

    BspDisplay_CommandData(0xB1U, normal_mode, sizeof(normal_mode));
    BspDisplay_CommandData(0xB2U, idle_mode, sizeof(idle_mode));
    BspDisplay_CommandData(0xB3U, partial_mode, sizeof(partial_mode));
    BspDisplay_CommandData(0xB4U, dot_inversion, sizeof(dot_inversion));
    BspDisplay_CommandData(0xC0U, power, sizeof(power));
    BspDisplay_CommandData(0xC1U, vgh_vgl, sizeof(vgh_vgl));
    BspDisplay_CommandData(0xC2U, normal_power, sizeof(normal_power));
    BspDisplay_CommandData(0xC3U, idle_power, sizeof(idle_power));
    BspDisplay_CommandData(0xC4U, partial_power, sizeof(partial_power));
    BspDisplay_CommandData(0xC5U, vcom, sizeof(vcom));
    BspDisplay_CommandData(0xE0U, gamma_positive, sizeof(gamma_positive));
    BspDisplay_CommandData(0xE1U, gamma_negative, sizeof(gamma_negative));
    BspDisplay_CommandData(0x3AU, color_mode, sizeof(color_mode));
    BspDisplay_CommandData(0x36U, madctl, sizeof(madctl));
    BspDisplay_CommandData(0x21U, NULL, 0U);
    BspDisplay_CommandData(0x29U, NULL, 0U);

    if (!s_spi_ok)
    {
        return false;
    }

    PlatformDisplay_FillRect(0, 0, APP_VIEW_WIDTH, APP_VIEW_HEIGHT,
                             UI_COLOR_BLACK);
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_BACKLIGHT_PIN,
                      (LCD_BACKLIGHT_ACTIVE_HIGH != 0) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return s_spi_ok;
}
