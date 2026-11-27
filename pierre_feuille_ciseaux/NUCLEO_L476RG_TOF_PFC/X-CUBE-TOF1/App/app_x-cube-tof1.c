/**
  ******************************************************************************
  * @file          : app_x-cube-tof1.c
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
  *
  * MODIFIÉ (hackathon) : l'exemple ST "threshold detection + NanoEdge" est
  * remplacé par l'application (Src/app.c). main.c n'est pas modifié : il
  * appelle toujours MX_X_CUBE_TOF1_Init() puis MX_X_CUBE_TOF1_Process() en boucle.
  *
  * Les RÉGLAGES DU CAPTEUR de l'exemple ST sont conservés ici, avec leurs
  * commentaires d'origine (projet ST NUCLEO_L476RG_TOF_VL53L5A1, fourni dans
  * ST-exemple/NUCLEO_L476RG_TOF_VL53L5A1.zip) : SUCCESSIVE_MATRIX,
  * MATRIX_RESOLUTION, AXES, NEAI_BUFFER_SIZE, MIN/MAX_THRESHOLD, NEAI_MODE, profil
  * (Profile.*) et interruption (ITConfig.*). Ils s'appliquent vraiment :
  * TOF1_SensorConfig() configure le capteur avec eux, et Src/tof_sensor.c l'appelle.
  * Différences voulues avec l'exemple ST :
  *  - ITConfig.Criteria = RS_IT_DEFAULT (une interruption par trame) au lieu de
  *    RS_IT_IN_WINDOW : le jeu a besoin de toutes les trames (fenêtre glissante,
  *    "En attente d'une main") ; les seuils restent ceux de la fenêtre de distance ;
  *  - MIN_THRESHOLD = 40 mm au lieu de 100 : une main peut venir à 4 cm ;
  *  - MAX_THRESHOLD = 200 mm au lieu de 400 : main entre 4 et 20 cm (voir
  *    ../REGLAGE_DISTANCE.md) ;
  *  - NEAI_MODE suit APP_MODE (Inc/app_config.h) ;
  *  - cible la plus proche (TOF_TARGET_CLOSEST) : la main plutôt que le plafond.
  * Les réglages aussi utilisés par le reste de l'application (Inc/app_config.h) sont
  * vérifiés à la compilation : une valeur modifiée d'un seul côté donne un #error.
  *
  * Les appels sont dans des blocs USER CODE, mais NE PAS régénérer le code
  * depuis le .ioc sans vérifier ce fichier : CubeMX y recréerait l'exemple ST.
  */

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "app_x-cube-tof1.h"
#include "main.h"
#include "app.h"
#include "app_config.h"

#include "53l5a1_ranging_sensor.h"
#include "app_tof_pin_conf.h"

#include "NanoEdgeAI.h"

/*
 * The purpose of this sample application is to showcase the threshold detection
 * functionality of this device.
 *
 * When the device detects a target that match the configuration an IT is generated
 * and the host will start printing the measurement information on the serial connection (UART).
 *
 * Here is the default configuration:
 *
 * ITConfig.Criteria = RS_IT_IN_WINDOW;
 * ITConfig.LowThreshold = 200; // distance in mm
 * ITConfig.HighThreshold = 600; // distance in mm
 *
 * Other availables interrupt generation criteria for this device are:
 *
 * - RS_IT_DEFAULT          // IT if new measurement is ready (no thresholds)
 * - RS_IT_IN_WINDOW        // IT if distance > thresh_high
 * - RS_IT_OUT_OF_WINDOW    // IT if distance < LowThreshold OR distance > HighThreshold
 * - RS_IT_BELOW_LOW        // IT if distance <= LowThreshold
 * - RS_IT_ABOVE_HIGH       // IT if distance > HighThreshold
 * - RS_IT_EQUAL_LOW        // IT if distance == LowThreshold
 * - RS_IT_NOT_EQUAL_LOW    // IT if distance != LowThreshold
 */
/* Pierre-Feuille-Ciseaux : RS_IT_DEFAULT (voir TOF1_SensorConfig()). */

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
/* Les 7 lignes suivantes sont celles du fichier de ST (dans le zip), où elles sont
 * ACTIVES (NEAI_MODE 1) ; elles sont gardées ici en commentaire, pour mémoire. Les
 * réglages appliqués suivent, présentés comme dans ST-exemple/README.md.          */
//#define SUCCESSIVE_MATRIX	8						/* How many successive matrix we get in our neai_buffer. Possible values: power of 2 between 1 and 64 */
//#define MATRIX_RESOLUTION	64 						/* Possible values: 64 for 8x8 resolution, 16 for 4x4 resolution */
//#define AXES				1						/* Possible value: 1 (distance data) */
//#define NEAI_BUFFER_SIZE	(SUCCESSIVE_MATRIX * MATRIX_RESOLUTION * AXES)	/* The size of our NEAI buffer used for classification */
//#define MIN_THRESHOLD		100						/* Minimum distance in mm between sensor & object to detect. Possible values: 40 to 4000 */
//#define MAX_THRESHOLD		400 					/* Maximum distance in mm between sensor & object to detect. Possible values: 40 to 4000 */
//#define NEAI_MODE			1						/* Possible values: 0 for data logging mode, 1 for NanoEdgeAI classification mode */

#define SUCCESSIVE_MATRIX 8 /* How many successive matrix we get in our neai_buffer. Possible values: power of 2 between 1 and 64 */
#define MATRIX_RESOLUTION 64 /* Possible values: 64 for 8x8 resolution, 16 for 4x4 resolution */
#define AXES 1 /* Possible value: 1 (distance data) */
#define NEAI_BUFFER_SIZE SUCCESSIVE_MATRIX * MATRIX_RESOLUTION * AXES /* The size of our NEAI buffer used for classification */
#define MIN_THRESHOLD 40 /* Minimum distance in mm between sensor & object to detect. Possible values: 40 to 4000 */
#define MAX_THRESHOLD 200 /* Maximum distance in mm between sensor & object to detect. Possible values: 40 to 4000 */
#define NEAI_MODE APP_MODE /* Possible values: 0 for data logging mode, 1 for NanoEdgeAI classification mode */


#if (SUCCESSIVE_MATRIX < 1) || (SUCCESSIVE_MATRIX > 64) || \
    ((SUCCESSIVE_MATRIX & (SUCCESSIVE_MATRIX - 1)) != 0)
#error SUCCESSIVE_MATRIX must be a power of two between 1 and 64
#endif
#if (MATRIX_RESOLUTION != 16) && (MATRIX_RESOLUTION != 64)
#error MATRIX_RESOLUTION must be 16 or 64
#endif
#if (AXES != 1) || ((NEAI_MODE != 0) && (NEAI_MODE != 1))
#error AXES must be 1 and NEAI_MODE must be 0 or 1
#endif
#if (MIN_THRESHOLD < 40) || (MAX_THRESHOLD > 4000) || (MIN_THRESHOLD > MAX_THRESHOLD)
#error Invalid distance thresholds
#endif
/* (Pierre-Feuille-Ciseaux : contrôle fait seulement avec l'API récente de NanoEdge ;
 *  Src/rps_ai.c vérifie aussi la taille avec l'ancienne.)                          */
#if defined(NEAI_INPUT_SIGNAL_LENGTH)
#if NEAI_MODE && (NEAI_BUFFER_SIZE != (NEAI_INPUT_SIGNAL_LENGTH * NEAI_INPUT_AXIS_NUMBER))
#error The sensor buffer must match the NanoEdgeAI model input size
#endif
#endif

/* Pierre-Feuille-Ciseaux : ces réglages doivent concorder avec Inc/app_config.h,
 * utilisé par le reste de l'application (prétraitement, jeu, tests sur PC).     */
#if SUCCESSIVE_MATRIX != PRE_FRAMES_PER_SIGNAL
#error SUCCESSIVE_MATRIX doit etre egal a PRE_FRAMES_PER_SIGNAL (Inc/app_config.h)
#endif
#if MATRIX_RESOLUTION != TOF_ZONES
#error MATRIX_RESOLUTION doit etre egal a TOF_ZONES (Inc/app_config.h) : application travaille en 8x8
#endif
#if (NEAI_BUFFER_SIZE) != SIGNAL_LEN
#error NEAI_BUFFER_SIZE doit etre egal a SIGNAL_LEN (Inc/app_config.h)
#endif
#if (MIN_THRESHOLD != PRE_NEAR_MM) || (MAX_THRESHOLD != PRE_FAR_MM)
#error MIN_THRESHOLD / MAX_THRESHOLD doivent etre egaux a PRE_NEAR_MM / PRE_FAR_MM (Inc/app_config.h)
#endif
#if TOF_FREQ_HZ != 15
#error Profile.Frequency vaut 15 Hz ici : TOF_FREQ_HZ (Inc/app_config.h) doit suivre
#endif

/* Private variables ---------------------------------------------------------*/
static RANGING_SENSOR_Capabilities_t Cap;
static RANGING_SENSOR_ProfileConfig_t Profile;
static int32_t status = 0;
/* Positionné par HAL_GPIO_EXTI_Callback() (X-CUBE-TOF1/Target/app_tof_pin_conf.c)
 * sur l'interruption INT du capteur (PA4) ; utilisé par Src/tof_sensor.c.    */
volatile uint8_t ToF_EventDetected = 0;
/* (Les variables NanoEdge de l'exemple ST, neai_buffer, id_class..., sont dans
 *  Src/rps_ai.c, qui fait la classification du jeu.)                          */

void MX_X_CUBE_TOF1_Init(void)
{
  /* USER CODE BEGIN SV */

  /* USER CODE END SV */

  /* USER CODE BEGIN X_CUBE_TOF1_Init_PreTreatment */
  app_init();   /* appelle tof_sensor_init(), donc TOF1_SensorConfig() */
  /* USER CODE END X_CUBE_TOF1_Init_PreTreatment */

  /* USER CODE BEGIN X_CUBE_TOF1_Init_PostTreatment */

  /* USER CODE END X_CUBE_TOF1_Init_PostTreatment */
}

/*
 * LM background task
 */
void MX_X_CUBE_TOF1_Process(void)
{
  /* USER CODE BEGIN X_CUBE_TOF1_Process_PreTreatment */
  app_loop();
  /* USER CODE END X_CUBE_TOF1_Process_PreTreatment */

  /* USER CODE BEGIN X_CUBE_TOF1_Process_PostTreatment */

  /* USER CODE END X_CUBE_TOF1_Process_PostTreatment */
}

/*
 * Pierre-Feuille-Ciseaux : configuration du capteur central avec les réglages ci-dessus,
 * dans l'ordre de MX_53L5A1_ThresholdDetection_Init() / _Process() de l'exemple ST, puis
 * démarrage de la mesure continue. Retourne 0 si tout va bien, sinon un code d'erreur
 * (affiché par app.c) : 1 init, 2 profil, 3 cible la plus proche, 4 démarrage,
 * 5 interruption, 6 identifiant / capacités.
 */
int32_t TOF1_SensorConfig(void)
{
  uint32_t Id;
  RANGING_SENSOR_ITConfig_t ITConfig;

  if (VL53L5A1_RANGING_SENSOR_Init(VL53L5A1_DEV_CENTER) != BSP_ERROR_NONE)
  {
    return 1;                                 /* capteur absent / I2C */
  }
  if ((VL53L5A1_RANGING_SENSOR_ReadID(VL53L5A1_DEV_CENTER, &Id) != BSP_ERROR_NONE) ||
      (VL53L5A1_RANGING_SENSOR_GetCapabilities(VL53L5A1_DEV_CENTER, &Cap) != BSP_ERROR_NONE))
  {
    return 6;
  }

#if MATRIX_RESOLUTION == 64
  Profile.RangingProfile = RS_PROFILE_8x8_CONTINUOUS;
#else
  Profile.RangingProfile = RS_PROFILE_4x4_CONTINUOUS;
#endif
  Profile.TimingBudget = 5; /* 5 ms < TimingBudget < 100 ms */
  Profile.Frequency = 15; /* Hz */
  Profile.EnableAmbient = 0; /* Enable: 1, Disable: 0 */
  Profile.EnableSignal = 1; /* Enable: 1, Disable: 0 */

  /* set the profile if different from default one */
  if (VL53L5A1_RANGING_SENSOR_ConfigProfile(VL53L5A1_DEV_CENTER, &Profile) != BSP_ERROR_NONE)
  {
    return 2;
  }

  /* threshold parameters */
  ITConfig.Criteria = RS_IT_DEFAULT; /* Pierre-Feuille-Ciseaux : IT à chaque trame (exemple ST : RS_IT_IN_WINDOW) */
  ITConfig.LowThreshold = MIN_THRESHOLD; /* mm */
  ITConfig.HighThreshold = MAX_THRESHOLD; /* mm */

  if (VL53L5A1_RANGING_SENSOR_ConfigIT(VL53L5A1_DEV_CENTER, &ITConfig) != BSP_ERROR_NONE)
  {
    return 5;
  }

#if TOF_TARGET_CLOSEST
  /* Pierre-Feuille-Ciseaux : la cible la plus proche (la main) plutôt que la plus forte.
   * Pas d'accès par le BSP : on passe par l'API ULD de l'objet composant. */
  VL53L5CX_Object_t *obj = (VL53L5CX_Object_t *)VL53L5A1_RANGING_SENSOR_CompObj[VL53L5A1_DEV_CENTER];
  if (vl53l5cx_set_target_order(&obj->Dev, VL53L5CX_TARGET_ORDER_CLOSEST) != VL53L5CX_STATUS_OK)
  {
    return 3;
  }
#endif

  status = VL53L5A1_RANGING_SENSOR_Start(VL53L5A1_DEV_CENTER, RS_MODE_ASYNC_CONTINUOUS);
  if (status != BSP_ERROR_NONE)
  {
    return 4;
  }

  /* Reset ToF event variable to avoid starting log at boot */
  ToF_EventDetected = 0;
  return 0;
}

#ifdef __cplusplus
}
#endif

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
