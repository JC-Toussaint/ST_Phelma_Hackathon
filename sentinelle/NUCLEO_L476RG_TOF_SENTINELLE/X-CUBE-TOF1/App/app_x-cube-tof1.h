/**
  ******************************************************************************
  * @file          : app_x-cube-tof1.h
  * @author        : IMG SW Application Team
  * @brief         : This file provides code for the configuration
  *                  of the STMicroelectronics.X-CUBE-TOF1.2.0.0 instances.
  ******************************************************************************
  *
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __APP_X_CUBE_TOF1_H
#define __APP_X_CUBE_TOF1_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Exported defines ----------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/
void MX_X_CUBE_TOF1_Init(void);
void MX_X_CUBE_TOF1_Process(void);
/* Sentinelle : configure et démarre le capteur avec les réglages de l'exemple ST
 * conservés dans app_x-cube-tof1.c ; 0 si OK, sinon un code d'erreur. */
int32_t TOF1_SensorConfig(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_X_CUBE_TOF1_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
