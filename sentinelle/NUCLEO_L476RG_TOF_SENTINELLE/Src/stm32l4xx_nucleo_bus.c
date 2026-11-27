#include "stm32l4xx_nucleo_bus.h"

#define BUS_I2C1_POLL_TIMEOUT 0x1000U

static uint32_t i2c1_users;

int32_t BSP_I2C1_Init(void)
{
  if (i2c1_users == 0U)
  {
    /* The sensor's I2C recovery temporarily changes SCL and SDA to GPIO. */
    if ((HAL_I2C_DeInit(&hi2c1) != HAL_OK) ||
        (HAL_I2C_Init(&hi2c1) != HAL_OK) ||
        (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK) ||
        (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0U) != HAL_OK))
    {
      return BSP_ERROR_BUS_FAILURE;
    }
  }

  i2c1_users++;
  return BSP_ERROR_NONE;
}

int32_t BSP_I2C1_DeInit(void)
{
  if (i2c1_users == 0U)
  {
    return BSP_ERROR_NO_INIT;
  }
  if ((i2c1_users == 1U) && (HAL_I2C_DeInit(&hi2c1) != HAL_OK))
  {
    return BSP_ERROR_BUS_FAILURE;
  }

  i2c1_users--;
  return BSP_ERROR_NONE;
}

int32_t BSP_I2C1_WriteReg16(uint16_t address, uint16_t reg, uint8_t *data, uint16_t length)
{
  if (HAL_I2C_Mem_Write(&hi2c1, address, reg, I2C_MEMADD_SIZE_16BIT,
                        data, length, BUS_I2C1_POLL_TIMEOUT) == HAL_OK)
  {
    return BSP_ERROR_NONE;
  }
  return (HAL_I2C_GetError(&hi2c1) == HAL_I2C_ERROR_AF)
           ? BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE : BSP_ERROR_PERIPH_FAILURE;
}

int32_t BSP_I2C1_ReadReg16(uint16_t address, uint16_t reg, uint8_t *data, uint16_t length)
{
  if (HAL_I2C_Mem_Read(&hi2c1, address, reg, I2C_MEMADD_SIZE_16BIT,
                       data, length, BUS_I2C1_POLL_TIMEOUT) == HAL_OK)
  {
    return BSP_ERROR_NONE;
  }
  return (HAL_I2C_GetError(&hi2c1) == HAL_I2C_ERROR_AF)
           ? BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE : BSP_ERROR_PERIPH_FAILURE;
}

int32_t BSP_GetTick(void)
{
  return (int32_t)HAL_GetTick();
}
