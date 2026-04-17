/**
 * @file bmp180.c
 * @brief Driver implementation for the BMP180 digital pressure and temperature sensor.
 * @author rabilint
 * @date 04.04.26
 */

#include "bmp180.h"
#include <math.h>

/**
 * @brief Starts non-blocking (interrupt/DMA-based) measurement cycle.
 * @param dev Pointer to the BMP180 device structure.
 * @param oss Oversampling setting (0 to 3).
 */
void BMP180_Start_IRead(BMP180_t *dev, uint8_t oss)
{
    if (dev->state == BMP180_IDLE || dev->state == BMP180_DATA_READY) {
        dev->current_oss = oss;
        dev->state = BMP180_START_TEMP;
    }
}

/**
 * @brief Initializes the BMP180 sensor and reads calibration data.
 * @param dev Pointer to the BMP180 device structure.
 * @param hi2c Pointer to the I2C handle used for communication.
 * @param AtmospherePressure Sea-level pressure in Pa used for altitude calculation.
 * @return 0 on success, or 1 on failure.
 */
uint8_t BMP180_Init(BMP180_t *dev, I2C_HandleTypeDef *hi2c, float AtmospherePressure)
{
    dev->hi2c = hi2c;
    dev->AtmospherePressure = AtmospherePressure;
    uint8_t chip_id = 0;

    /* Verify Chip ID */
    if (HAL_I2C_Mem_Read(dev->hi2c, BMP180_ADDR, 0xD0, 1, &chip_id, 1, 100) != HAL_OK || chip_id != 0x55) {
        return 1;
    }

    /* Read calibration data (22 bytes starting at 0xAA) */
    uint8_t calib[22] = {0};
    for(uint8_t i = 0; i < 22; i++) {
        if(HAL_I2C_Mem_Read(dev->hi2c, BMP180_ADDR, 0xAA + i, I2C_MEMADD_SIZE_8BIT, &calib[i], 1, 50) != HAL_OK) {
            return 1;
        }
    }

    /* Parse calibration parameters */
    dev->AC1 = (int16_t)((calib[0] << 8) | calib[1]);
    dev->AC2 = (int16_t)((calib[2] << 8) | calib[3]);
    dev->AC3 = (int16_t)((calib[4] << 8) | calib[5]);
    dev->AC4 = (uint16_t)((calib[6] << 8) | calib[7]);
    dev->AC5 = (uint16_t)((calib[8] << 8) | calib[9]);
    dev->AC6 = (uint16_t)((calib[10] << 8) | calib[11]);
    dev->B1  = (int16_t)((calib[12] << 8) | calib[13]);
    dev->B2  = (int16_t)((calib[14] << 8) | calib[15]);
    dev->MB  = (int16_t)((calib[16] << 8) | calib[17]);
    dev->MC  = (int16_t)((calib[18] << 8) | calib[19]);
    dev->MD  = (int16_t)((calib[20] << 8) | calib[21]);

    return 0;
}

/**
 * @brief State machine update function for non-blocking operations.
 * Must be called periodically (e.g., in a main loop or timer callback).
 * @param dev Pointer to the BMP180 device structure.
 */
void BMP180_Update(BMP180_t *dev)
{
    uint8_t cmd;
    uint32_t delay;
    switch (dev->state)
    {
        case BMP180_IDLE:
            break;

        case BMP180_START_TEMP:
            /* Request temperature measurement */
            cmd = 0x2E;
            if(HAL_I2C_Mem_Write_IT(dev->hi2c, BMP180_ADDR, 0xF4, 1, &cmd, 1) == HAL_OK) {
                dev->start_tick = HAL_GetTick();
                dev->state = BMP180_WAIT_TEMP_DELAY;
            }
            break;

        case BMP180_WAIT_TEMP_DELAY:
            /* Wait for the mandatory 5ms for temperature measurement */
            if (HAL_GetTick() - dev->start_tick >= 5)
            {
                dev->rx_cplt_flag = 0;
                if (HAL_I2C_Mem_Read_DMA(dev->hi2c, BMP180_ADDR, 0xF6, I2C_MEMADD_SIZE_8BIT, dev->i2c_buffer, 2) == HAL_OK)
                {
                    dev->state = BMP180_WAIT_TEMP_DMA;
                }
            }
            break;

        case BMP180_WAIT_TEMP_DMA:
            /* Wait for the DMA receive complete flag */
            if (dev->rx_cplt_flag)
            {
                dev->rx_cplt_flag = 0;
                dev->UT = (dev->i2c_buffer[0] << 8) | dev->i2c_buffer[1];

                /* Calculate true temperature */
                int32_t X1 = ((dev->UT - (int32_t)dev->AC6) * (int32_t)dev->AC5) / 32768;
                int32_t X2 = ((int32_t)dev->MC * 2048) / (X1 + dev->MD);
                dev->B5 = X1 + X2;
                dev->temperature = (dev->B5 + 8) / 160.0f;

                dev->state = BMP180_START_PRESSURE;
            }
            break;

        case BMP180_START_PRESSURE:
            /* Request pressure measurement */
            cmd = 0x34 + (dev->current_oss << 6);
            if(HAL_I2C_Mem_Write_IT(dev->hi2c, BMP180_ADDR, 0xF4, 1, &cmd, 1) == HAL_OK)
            {
                dev->start_tick = HAL_GetTick();
                dev->state = BMP180_WAIT_PRESSURE_DELAY;
            }
            break;

        case BMP180_WAIT_PRESSURE_DELAY:
            /* Wait for the corresponding pressure conversion time based on oss */
            delay = (dev->current_oss == 0) ? 5 : (dev->current_oss == 1) ? 8 : (dev->current_oss == 2) ? 14 : 26;
            if (HAL_GetTick() - dev->start_tick >= delay)
            {
                dev->rx_cplt_flag = 0;
                if (HAL_I2C_Mem_Read_DMA(dev->hi2c, BMP180_ADDR, 0xF6, I2C_MEMADD_SIZE_8BIT, dev->i2c_buffer, 3) == HAL_OK)
                {
                    dev->state = BMP180_WAIT_PRESSURE_DMA;
                }
            }
            break;

        case BMP180_WAIT_PRESSURE_DMA:
            /* Wait for the DMA receive complete flag */
            if (dev->rx_cplt_flag)
            {
                dev->rx_cplt_flag = 0;
                dev->state = BMP180_CALCULATE;
            }
            break;

        case BMP180_CALCULATE:
        {
            /* Compute true pressure from uncompensated data and calibration factors */
            int32_t X1, X2, X3, B3, B6, p;
            uint32_t B4, B7;

            dev->UP = (((uint32_t)dev->i2c_buffer[0] << 16) | ((uint32_t)dev->i2c_buffer[1] << 8) | dev->i2c_buffer[2]) >> (8 - dev->current_oss);
            B6 = dev->B5 - 4000;

            X1 = ((int32_t)dev->B2 * ((B6 * B6) / 4096)) / 2048;
            X2 = ((int32_t)dev->AC2 * B6) / 2048;
            X3 = X1 + X2;
            B3 = (((((int32_t)dev->AC1 * 4) + X3) << dev->current_oss) + 2) / 4;

            X1 = ((int32_t)dev->AC3 * B6) / 8192;
            X2 = ((int32_t)dev->B1 * ((B6 * B6) / 4096)) / 65536;
            X3 = ((X1 + X2) + 2) / 4;
            B4 = ((uint32_t)dev->AC4 * (uint32_t)(X3 + 32768)) / 32768;

            B7 = ((uint32_t)dev->UP - (uint32_t)B3) * (50000 >> dev->current_oss);
            if (B7 < 0x80000000) p = (B7 * 2) / B4;
            else p = (B7 / B4) * 2;

            X1 = (p / 256) * (p / 256);
            X1 = (X1 * 3038) / 65536;
            X2 = (-7357 * p) / 65536;
            dev->pressure = p + ((X1 + X2 + 3791) / 16);

            /* Calculate altitude if reference pressure is valid */
            if (dev->AtmospherePressure > 0)
            {
                float pressure_ratio = (float)dev->pressure / dev->AtmospherePressure;
                dev->Alt = 44330.0f * (1.0f - powf(pressure_ratio, 0.19029495718f));
            }
            dev->state = BMP180_DATA_READY;
            break;
        }
        case BMP180_DATA_READY:
            break;
    }
}

/**
 * @brief Performs a blocking read of temperature.
 * @param dev Pointer to the BMP180 device structure.
 */
void BMP180_GetTemp_Blocking(BMP180_t *dev)
{
    uint8_t data[2] = {0};
    uint8_t cmd;
    int32_t UT, X1, X2;

    /* Request uncompensated temperature */
    cmd = 0x2E;
    if (HAL_I2C_Mem_Write(dev->hi2c, BMP180_ADDR, 0xF4, I2C_MEMADD_SIZE_8BIT, &cmd, 1, 100) != HAL_OK) return;
    HAL_Delay(5);

    /* Read uncompensated temperature */
    if (HAL_I2C_Mem_Read(dev->hi2c, BMP180_ADDR, 0xF6, I2C_MEMADD_SIZE_8BIT, data, 2, 100) != HAL_OK) return;
    UT = (data[0] << 8) | data[1];

    /* Calculate true temperature */
    X1 = ((UT - (int32_t)dev->AC6) * (int32_t)dev->AC5) / 32768;
    X2 = ((int32_t)dev->MC * 2048) / (X1 + dev->MD);
    dev->B5 = X1 + X2;
    dev->temperature = (dev->B5 + 8) / 160.0f;
}

/**
 * @brief Performs a blocking read of pressure.
 * @param dev Pointer to the BMP180 device structure.
 * @param oss Oversampling setting (0 to 3).
 */
void BMP180_GetPressure_Blocking(BMP180_t *dev, uint8_t oss)
{
    uint8_t data[3] = {0};
    uint8_t cmd;
    int32_t UP, X1, X2, X3, B3, B6, p;
    uint32_t B4, B7;

    /* Request uncompensated pressure */
    cmd = 0x34 + (oss << 6);
    if (HAL_I2C_Mem_Write(dev->hi2c, BMP180_ADDR, 0xF4, I2C_MEMADD_SIZE_8BIT, &cmd, 1, 100) != HAL_OK) return;

    /* Wait based on oversampling setting */
    uint32_t delays[] = {5, 8, 14, 26};
    HAL_Delay(delays[oss & 0x03] + 2);

    /* Read uncompensated pressure */
    if (HAL_I2C_Mem_Read(dev->hi2c, BMP180_ADDR, 0xF6, I2C_MEMADD_SIZE_8BIT, data, 3, 100) != HAL_OK) return;
    UP = (((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | data[2]) >> (8 - oss);

    /* Calculate true pressure */
    B6 = dev->B5 - 4000;
    X1 = ((int32_t)dev->B2 * ((B6 * B6) / 4096)) / 2048;
    X2 = ((int32_t)dev->AC2 * B6) / 2048;
    X3 = X1 + X2;
    B3 = (((((int32_t)dev->AC1 * 4) + X3) << oss) + 2) / 4;

    X1 = ((int32_t)dev->AC3 * B6) / 8192;
    X2 = ((int32_t)dev->B1 * ((B6 * B6) / 4096)) / 65536;
    X3 = ((X1 + X2) + 2) / 4;
    B4 = ((uint32_t)dev->AC4 * (uint32_t)(X3 + 32768)) / 32768;

    B7 = ((uint32_t)UP - (uint32_t)B3) * (50000 >> oss);
    if (B7 < 0x80000000) p = (B7 * 2) / B4;
    else p = (B7 / B4) * 2;

    X1 = (p / 256) * (p / 256);
    X1 = (X1 * 3038) / 65536;
    X2 = (-7357 * p) / 65536;
    dev->pressure = p + ((X1 + X2 + 3791) / 16);
}

/**
 * @brief Calculates absolute altitude from measured pressure and sea-level pressure.
 * @param pressure Measured pressure in Pa.
 * @param sea_level_pressure Reference sea-level pressure in Pa.
 * @return Calculated altitude in meters.
 */
float BMP180_GetAltitude(int32_t pressure, float sea_level_pressure)
{
    if (sea_level_pressure > 0)
    {
        float pressure_ratio = (float)pressure / sea_level_pressure;
        return 44330.0f * (1.0f - powf(pressure_ratio, 0.19029495718f));
    }
    return 0.0f;
}

/**
 * @brief Performs a blocking read of both temperature and pressure.
 * @param dev Pointer to the BMP180 device structure.
 * @param oss Oversampling setting (0: ultra low power, 3: ultra high resolution).
 */
void BMP180_Read_Blocking(BMP180_t *dev, uint8_t oss)
{
    BMP180_GetTemp_Blocking(dev);
    BMP180_GetPressure_Blocking(dev, oss);
    dev->Alt = BMP180_GetAltitude(dev->pressure, dev->AtmospherePressure);
}
