#ifndef TEST_STM32L4XX_HAL_H
#define TEST_STM32L4XX_HAL_H

#include <stdint.h>

typedef struct { int unused; } I2C_HandleTypeDef;
typedef enum { HAL_OK = 0, HAL_ERROR = 1 } HAL_StatusTypeDef;

#define I2C_ANALOGFILTER_ENABLE 0U
#define I2C_MEMADD_SIZE_16BIT 2U
#define HAL_I2C_ERROR_AF 4U

HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef *handle);
HAL_StatusTypeDef HAL_I2C_DeInit(I2C_HandleTypeDef *handle);
HAL_StatusTypeDef HAL_I2CEx_ConfigAnalogFilter(I2C_HandleTypeDef *handle, uint32_t filter);
HAL_StatusTypeDef HAL_I2CEx_ConfigDigitalFilter(I2C_HandleTypeDef *handle, uint32_t filter);
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *handle, uint16_t address,
                                   uint16_t reg, uint16_t reg_size, uint8_t *data,
                                   uint16_t length, uint32_t timeout);
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *handle, uint16_t address,
                                  uint16_t reg, uint16_t reg_size, uint8_t *data,
                                  uint16_t length, uint32_t timeout);
uint32_t HAL_I2C_GetError(I2C_HandleTypeDef *handle);
uint32_t HAL_GetTick(void);

#endif
