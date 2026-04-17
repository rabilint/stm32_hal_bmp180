/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : 1-ex_sync_method.c
  * @brief          : Example of using the BMP180 sensor in blocking (synchronous) mode.
  ******************************************************************************
  */
/* USER CODE END Header */

/* USER CODE BEGIN Includes */
#include "bmp180.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* USER CODE BEGIN PV */
BMP180_t bmp180;          /**< BMP180 device context */
char uart_buf[64];        /**< Buffer for formatting UART messages */
/* USER CODE END PV */

int main(void)
{
    /* HAL initialization and system configuration */
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_USART1_UART_Init();

    /* USER CODE BEGIN 2 */
    /**
     * Initialize the BMP180 sensor.
     * Passes the device context, the I2C handle, and the reference sea-level pressure (in Pa).
     */
    if (BMP180_Init(&bmp180, &hi2c1, 101325.0f) != 0) {
        char err[] = "ERR: INIT FAIL\r\n";
        HAL_UART_Transmit(&huart1, (uint8_t*)err, strlen(err), 100);
        Error_Handler();
    }
    /* USER CODE END 2 */

    /* Infinite loop */
    while (1)
    {
        /* USER CODE BEGIN 3 */

        /* Perform a blocking read of temperature, pressure, and calculate altitude using maximum oversampling (3) */
        BMP180_Read_Blocking(&bmp180, 3);

        /* Format temperature to print as a floating-point string using integers (e.g., 25.34) */
        int temp_w = (int)bmp180.temperature;
        int temp_f = (int)(bmp180.temperature * 100.0f) % 100;
        if (temp_f < 0) temp_f = -temp_f;

        /* Format altitude similarly */
        int alt_w = (int)bmp180.Alt;
        int alt_f = (int)(bmp180.Alt * 10.0f) % 10;
        if (alt_f < 0) alt_f = -alt_f;

        /* Prepare the string containing the sensor data */
        memset(uart_buf, 0, sizeof(uart_buf));
        int len = snprintf(uart_buf, sizeof(uart_buf), "T:%d.%02d P:%ld A:%d.%d\r\n",
                           temp_w, temp_f, bmp180.pressure, alt_w, alt_f);

        /* Transmit the data over UART */
        if (len > 0) {
            HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, len, 100);
        }

        /* Delay before the next reading */
        HAL_Delay(500);
        /* USER CODE END 3 */
    }
}