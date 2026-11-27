#include <assert.h>
#include <stddef.h>

#include "stm32l4xx_nucleo_bus.h"

I2C_HandleTypeDef hi2c1;
static unsigned init_calls, deinit_calls, analog_calls, digital_calls;
static HAL_StatusTypeDef deinit_status, init_status, analog_status, digital_status, transfer_status;
static uint32_t i2c_error;

HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef *handle)
{
  assert(handle == &hi2c1);
  init_calls++;
  return init_status;
}

HAL_StatusTypeDef HAL_I2C_DeInit(I2C_HandleTypeDef *handle)
{
  assert(handle == &hi2c1);
  deinit_calls++;
  return deinit_status;
}

HAL_StatusTypeDef HAL_I2CEx_ConfigAnalogFilter(I2C_HandleTypeDef *handle, uint32_t filter)
{
  assert(handle == &hi2c1 && filter == I2C_ANALOGFILTER_ENABLE);
  analog_calls++;
  return analog_status;
}

HAL_StatusTypeDef HAL_I2CEx_ConfigDigitalFilter(I2C_HandleTypeDef *handle, uint32_t filter)
{
  assert(handle == &hi2c1 && filter == 0);
  digital_calls++;
  return digital_status;
}

HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *handle, uint16_t address,
                                   uint16_t reg, uint16_t reg_size, uint8_t *data,
                                   uint16_t length, uint32_t timeout)
{
  assert(handle == &hi2c1 && address == 0x52 && reg == 0x1234);
  assert(reg_size == I2C_MEMADD_SIZE_16BIT && data != NULL && length == 1);
  assert(timeout == 0x1000U);
  return transfer_status;
}

HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *handle, uint16_t address,
                                  uint16_t reg, uint16_t reg_size, uint8_t *data,
                                  uint16_t length, uint32_t timeout)
{
  return HAL_I2C_Mem_Write(handle, address, reg, reg_size, data, length, timeout);
}

uint32_t HAL_I2C_GetError(I2C_HandleTypeDef *handle)
{
  assert(handle == &hi2c1);
  return i2c_error;
}

uint32_t HAL_GetTick(void)
{
  return 123;
}

int main(void)
{
  uint8_t data = 0;

  assert(BSP_I2C1_DeInit() == BSP_ERROR_NO_INIT);
  analog_status = HAL_ERROR;
  assert(BSP_I2C1_Init() == BSP_ERROR_BUS_FAILURE);
  assert(BSP_I2C1_DeInit() == BSP_ERROR_NO_INIT);
  analog_status = HAL_OK;
  assert(BSP_I2C1_Init() == BSP_ERROR_NONE);
  assert(init_calls == 2 && deinit_calls == 2 && analog_calls == 2 && digital_calls == 1);
  assert(BSP_I2C1_Init() == BSP_ERROR_NONE);
  assert(init_calls == 2);

  assert(BSP_I2C1_WriteReg16(0x52, 0x1234, &data, 1) == BSP_ERROR_NONE);
  assert(BSP_I2C1_ReadReg16(0x52, 0x1234, &data, 1) == BSP_ERROR_NONE);
  transfer_status = HAL_ERROR;
  i2c_error = HAL_I2C_ERROR_AF;
  assert(BSP_I2C1_WriteReg16(0x52, 0x1234, &data, 1) == BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE);
  assert(BSP_I2C1_ReadReg16(0x52, 0x1234, &data, 1) == BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE);
  i2c_error = 0;
  assert(BSP_I2C1_ReadReg16(0x52, 0x1234, &data, 1) == BSP_ERROR_PERIPH_FAILURE);
  assert(BSP_GetTick() == 123);

  assert(BSP_I2C1_DeInit() == BSP_ERROR_NONE && deinit_calls == 2);
  deinit_status = HAL_ERROR;
  assert(BSP_I2C1_DeInit() == BSP_ERROR_BUS_FAILURE);
  deinit_status = HAL_OK;
  assert(BSP_I2C1_DeInit() == BSP_ERROR_NONE && deinit_calls == 4);
  assert(BSP_I2C1_DeInit() == BSP_ERROR_NO_INIT);
  return 0;
}
