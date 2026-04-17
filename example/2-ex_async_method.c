/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : 2-ex_async_method.c
  * @brief          : Example of using the BMP180 sensor in non-blocking (asynchronous) mode with DMA.
  ******************************************************************************
  */
/* USER CODE END Header */

/* USER CODE BEGIN Includes */
#include "bmp180.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* USER CODE BEGIN PV */
BMP180_t bmp180;                  /**< BMP180 device context */
char uart_tx_buffer[64];          /**< Buffer for formatting UART messages */
uint32_t last_read_tick = 0;      /**< Timestamp of the last successful sensor reading */
/* USER CODE END PV */

/* USER CODE BEGIN 0 */
/**
 * @brief I2C Memory Read Completion Callback.
 * This callback is invoked by the HAL when a DMA-based I2C read operation is complete.
 * It sets a flag in the BMP180 device context to signal the state machine.
 * @param hi2c Pointer to the I2C handle.
 */
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == bmp180.hi2c->Instance) {
        bmp180.rx_cplt_flag = 1;
    }
}
/* USER CODE END 0 */

int main(void)
{
    /* HAL initialization and system configuration */
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    /* DMA and peripheral initialization */
    MX_DMA_Init();
    MX_I2C1_Init();
    MX_USART1_UART_Init();

    /* USER CODE BEGIN 2 */
    /**
     * Initialize the BMP180 sensor.
     * Passes the device context, the I2C handle, and the reference sea-level pressure (in Pa).
     */
    if (BMP180_Init(&bmp180, &hi2c1, 101325.0f) != 0) {
        Error_Handler();
    }

    /* Start the first non-blocking measurement cycle */
    BMP180_Start_IRead(&bmp180, 3);
    /* USER CODE END 2 */

    /* Infinite loop */
    while (1)
    {
        /* USER CODE BEGIN 3 */

        /* Continuously update the BMP180 state machine. This handles the entire measurement process. */
        BMP180_Update(&bmp180);

        /* Check if the state machine has reached the data-ready state */
        if (bmp180.state == BMP180_DATA_READY)
        {
            /* Format temperature for printing */
            int temp_w = (int)bmp180.temperature;
            int temp_f = (int)(bmp180.temperature * 100.0f) % 100;
            if (temp_f < 0) temp_f = -temp_f;

            /* Format altitude for printing */
            int alt_w = (int)bmp180.Alt;
            int alt_f = (int)(bmp180.Alt * 10.0f) % 10;
            if (alt_f < 0) alt_f = -alt_f;

            /* Prepare the data string */
            memset(uart_tx_buffer, 0, sizeof(uart_tx_buffer));
            int len = snprintf(uart_tx_buffer, sizeof(uart_tx_buffer), "T:%d.%02d P:%ld A:%d.%d\r\n",
                               temp_w, temp_f, bmp180.pressure, alt_w, alt_f);

            /* Transmit the data over UART using DMA to avoid blocking */
            if (len > 0) {
                HAL_UART_Transmit_DMA(&huart1, (uint8_t*)uart_tx_buffer, len);
            }

            /* Reset the state machine to idle and record the time */
            bmp180.state = BMP180_IDLE;
            last_read_tick = HAL_GetTick();
        }

        /* If the sensor is idle and 500ms have passed, start a new measurement cycle */
        if (bmp180.state == BMP180_IDLE && (HAL_GetTick() - last_read_tick >= 500))
        {
            BMP180_Start_IRead(&bmp180, 3);
        }

        /* USER CODE END 3 */
    }
}