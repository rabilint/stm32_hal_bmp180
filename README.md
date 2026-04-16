# STM32 HAL BMP180 Driver

This project provides a simple and efficient driver for the BMP180 digital barometric pressure and temperature sensor, designed for use with STM32 microcontrollers leveraging the STM32Cube HAL library. It supports both blocking (synchronous) and non-blocking (asynchronous with DMA) operation modes.

## Description

The BMP180 is a high-precision digital barometric pressure sensor, also capable of measuring temperature. This driver simplifies its integration into STM32 projects by providing functions for initialization, reading sensor data, and calculating derived values like altitude. The asynchronous mode allows for efficient background measurements without blocking the main application flow, making it suitable for real-time embedded systems.

## Getting Started

### Dependencies

To use this library, you will need:

*   **STM32CubeIDE** or any other IDE/toolchain compatible with STM32 microcontrollers.
*   **STM32 HAL Library**: The project relies on the STM32Cube HAL for I2C communication, delays, and DMA.
*   **I2C Peripheral**: An I2C peripheral configured on your STM32 microcontroller.
*   **UART Peripheral (Optional)**: For debugging and outputting sensor data, as shown in the examples.
*   **DMA Controller (Optional, for asynchronous mode)**: If using the asynchronous measurement method, DMA must be configured for the I2C peripheral's receive operations.

### Installation

1.  **Add Source Files**: Copy `bmp180.c` and `bmp180.h` into your STM32CubeIDE project's `Core/Src` and `Core/Inc` folders (or equivalent locations for your project structure).
2.  **Include Header**: Include `bmp180.h` in your `main.c` or any other source file where you intend to use the driver.

    ```c
    #include "bmp180.h"
    ```

### Usage

#### 1. Initialization

Before using the sensor, it must be initialized. This involves passing an `I2C_HandleTypeDef` instance and a reference atmospheric pressure for altitude calculations.

```c
BMP180_t bmp180_dev; // Declare a BMP180 device structure
// ... (in main or a setup function) ...
if (BMP180_Init(&bmp180_dev, &hi2c1, 101325.0f) != 0) {
    // Handle initialization error (e.g., sensor not found, calibration read failed)
    Error_Handler();
}
```
*   `&hi2c1`: Replace with the actual I2C handle for your project (e.g., `&hi2c1`, `&hi2c2`).
*   `101325.0f`: This is the standard sea-level atmospheric pressure in Pascals. Adjust this value if you need more accurate altitude readings based on your local sea-level pressure.

#### 2. Synchronous (Blocking) Mode

This mode is simpler to use but will halt your program execution during I2C transactions and measurement delays.

*   **Read All Data (Temperature, Pressure, Altitude)**:
    ```c
    // In your main loop or a periodic task
    BMP180_Read_Blocking(&bmp180_dev, 3); // Read with Ultra High Resolution (OSS=3)

    // Access results
    float temperature = bmp180_dev.temperature; // Temperature in Celsius
    int32_t pressure = bmp180_dev.pressure;     // Pressure in Pascals
    float altitude = bmp180_dev.Alt;            // Altitude in meters
    ```
    The `oss` parameter (Oversampling Setting) can be 0 (ultra low power), 1 (standard), 2 (high), or 3 (ultra high resolution). Higher `oss` values provide better accuracy but require longer conversion times.

*   **Read Temperature Only**:
    ```c
    BMP180_GetTemp_Blocking(&bmp180_dev);
    float temperature = bmp180_dev.temperature;
    ```

*   **Read Pressure Only**:
    ```c
    BMP180_GetPressure_Blocking(&bmp180_dev, 3); // Read with Ultra High Resolution
    int32_t pressure = bmp180_dev.pressure;
    ```

*   **Calculate Altitude Separately**:
    ```c
    float current_altitude = BMP180_GetAltitude(bmp180_dev.pressure, bmp180_dev.AtmospherePressure);
    ```

#### 3. Asynchronous (Non-Blocking) Mode with DMA

This mode utilizes DMA for I2C data transfers and a state machine to manage the measurement process, allowing your main loop to perform other tasks.

**Prerequisites**:
*   Ensure I2C DMA is configured for receive operations in your STM32CubeIDE project.
*   Implement the `HAL_I2C_MemRxCpltCallback` in your `main.c` or a suitable HAL callback file:

    ```c
    void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
    {
        // Check if the callback is for your BMP180's I2C instance
        if (hi2c->Instance == bmp180_dev.hi2c->Instance) {
            bmp180_dev.rx_cplt_flag = 1; // Signal that DMA transfer is complete
        }
    }
    ```

**Usage**:

1.  **Start a Measurement Cycle**:
    ```c
    // Call this once to start the first measurement, or when you want a new reading
    BMP180_Start_IRead(&bmp180_dev, 3); // Start with Ultra High Resolution (OSS=3)
    ```

2.  **Update the State Machine Periodically**:
    Call `BMP180_Update` frequently in your main loop. This function drives the measurement process through its various states.

    ```c
    // In your main loop (while(1))
    BMP180_Update(&bmp180_dev);

    if (bmp180_dev.state == BMP180_DATA_READY) {
        // Data is ready! Access bmp180_dev.temperature, bmp180_dev.pressure, bmp180_dev.Alt
        // Process data, then reset state to IDLE to allow new measurements
        // For example, print data and then:
        bmp180_dev.state = BMP180_IDLE;
    }

    // Optionally, start a new measurement after a delay
    if (bmp180_dev.state == BMP180_IDLE && (HAL_GetTick() - last_read_tick >= 500)) {
        BMP180_Start_IRead(&bmp180_dev, 3);
        last_read_tick = HAL_GetTick(); // Update timestamp
    }
    ```

## Examples

Refer to the `example/` directory for complete usage examples:

*   `1-ex_sync_method.c`: Demonstrates basic synchronous (blocking) usage.
*   `2-ex_async_method.c`: Demonstrates advanced asynchronous (non-blocking) usage with DMA.

## Authors

*   **Mykyta Maiboroda | rabilint** - Initial work

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

*   Based on the BMP180 datasheet and various community examples for calibration algorithms.
