#ifndef STM32L4XX_NUCLEO_BUS_H
#define STM32L4XX_NUCLEO_BUS_H

#include "stm32l4xx_hal.h"
#include "stm32l4xx_nucleo_errno.h"

#define BUS_I2C1_SCL_GPIO_PORT GPIOB
#define BUS_I2C1_SCL_GPIO_PIN  GPIO_PIN_8
#define BUS_I2C1_SDA_GPIO_PORT GPIOB
#define BUS_I2C1_SDA_GPIO_PIN  GPIO_PIN_9

extern I2C_HandleTypeDef hi2c1;

int32_t BSP_I2C1_Init(void);
int32_t BSP_I2C1_DeInit(void);
int32_t BSP_I2C1_WriteReg16(uint16_t address, uint16_t reg, uint8_t *data, uint16_t length);
int32_t BSP_I2C1_ReadReg16(uint16_t address, uint16_t reg, uint8_t *data, uint16_t length);
int32_t BSP_GetTick(void);

#endif
