/**
 * @file bmp180.h
 * @brief Driver for the BMP180 digital pressure and temperature sensor.
 * @author rabilint
 * @date 04.04.26
 */

#ifndef BMP180_H
#define BMP180_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/**
 * @brief BMP180 I2C device address.
 * Left-aligned 7-bit address for STM32 HAL (0x77 << 1).
 */
#define BMP180_ADDR 0xEE

/**
 * @brief State machine states for non-blocking (DMA/Interrupt) operations.
 */
typedef enum {
    BMP180_IDLE,                 /**< Device is idle */
    BMP180_START_TEMP,           /**< Start temperature measurement */
    BMP180_WAIT_TEMP_DELAY,      /**< Waiting for temperature measurement delay */
    BMP180_WAIT_TEMP_DMA,        /**< Waiting for DMA temperature read completion */
    BMP180_START_PRESSURE,       /**< Start pressure measurement */
    BMP180_WAIT_PRESSURE_DELAY,  /**< Waiting for pressure measurement delay */
    BMP180_WAIT_PRESSURE_DMA,    /**< Waiting for DMA pressure read completion */
    BMP180_CALCULATE,            /**< Calculate true temperature and pressure */
    BMP180_DATA_READY            /**< New data is ready to be read */
} BMP180_State_t;

/**
 * @brief BMP180 device context structure.
 */
typedef struct {
    I2C_HandleTypeDef *hi2c;     /**< Pointer to I2C handle */

    /* Calibration parameters */
    int16_t AC1, AC2, AC3, B1, B2, MB, MC, MD;
    uint16_t AC4, AC5, AC6;
    int32_t B5;

    /* Output data */
    int32_t pressure;            /**< Calculated pressure in Pa */
    float temperature;           /**< Calculated temperature in Celsius */
    float Alt;                   /**< Calculated altitude in meters */
    float AtmospherePressure;    /**< Reference sea-level atmospheric pressure in Pa */

    volatile uint8_t rx_cplt_flag; /**< DMA receive complete flag */

    /* State machine context */
    BMP180_State_t state;        /**< Current state of the non-blocking state machine */
    uint32_t start_tick;         /**< Tick count for delay measurements */
    uint8_t i2c_buffer[3];       /**< Buffer for I2C DMA transactions */
    uint8_t current_oss;         /**< Current oversampling setting (0-3) */

    /* Raw sensor data */
    int32_t UT;                  /**< Uncompensated temperature value */
    int32_t UP;                  /**< Uncompensated pressure value */

} BMP180_t;

/**
 * @brief Initializes the BMP180 sensor and reads calibration data.
 * @param dev Pointer to the BMP180 device structure.
 * @param hi2c Pointer to the I2C handle used for communication.
 * @param AtmospherePressure Sea-level pressure in Pa used for altitude calculation.
 * @return 0 on success, or an error code on failure.
 */
uint8_t BMP180_Init(BMP180_t *dev, I2C_HandleTypeDef *hi2c, float AtmospherePressure);

/**
 * @brief Performs a blocking read of both temperature and pressure.
 * @param dev Pointer to the BMP180 device structure.
 * @param oss Oversampling setting (0: ultra low power, 3: ultra high resolution).
 */
void BMP180_Read_Blocking(BMP180_t *dev, uint8_t oss);

/**
 * @brief Calculates absolute altitude from measured pressure and sea-level pressure.
 * @param pressure Measured pressure in Pa.
 * @param sea_level_pressure Reference sea-level pressure in Pa.
 * @return Calculated altitude in meters.
 */
float BMP180_GetAltitude(int32_t pressure, float sea_level_pressure);

/**
 * @brief Performs a blocking read of pressure.
 * @param dev Pointer to the BMP180 device structure.
 * @param oss Oversampling setting (0 to 3).
 */
void BMP180_GetPressure_Blocking(BMP180_t *dev, uint8_t oss);

/**
 * @brief Performs a blocking read of temperature.
 * @param dev Pointer to the BMP180 device structure.
 */
void BMP180_GetTemp_Blocking(BMP180_t *dev);

/**
 * @brief Starts non-blocking (interrupt/DMA-based) measurement cycle.
 * @param dev Pointer to the BMP180 device structure.
 * @param oss Oversampling setting (0 to 3).
 */
void BMP180_Start_IRead(BMP180_t *dev, uint8_t oss);

/**
 * @brief State machine update function for non-blocking operations.
 * Must be called periodically (e.g., in a main loop or timer callback).
 * @param dev Pointer to the BMP180 device structure.
 */
void BMP180_Update(BMP180_t *dev);

#endif // BMP180_H
