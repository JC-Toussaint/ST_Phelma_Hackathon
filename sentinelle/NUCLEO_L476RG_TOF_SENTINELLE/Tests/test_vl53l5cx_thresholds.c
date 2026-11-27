#include <assert.h>
#include <string.h>

#include "vl53l5cx.h"

static uint8_t resolution;
static uint8_t resolution_status;
static uint8_t thresholds_status;
static unsigned thresholds_calls;
static unsigned enable_calls;
static uint8_t last_enabled;
static VL53L5CX_DetectionThresholds captured[VL53L5CX_NB_THRESHOLDS];

uint8_t __wrap_vl53l5cx_get_resolution(VL53L5CX_Configuration *dev, uint8_t *value)
{
  (void)dev;
  *value = resolution;
  return resolution_status;
}

uint8_t __wrap_vl53l5cx_set_detection_thresholds(VL53L5CX_Configuration *dev,
                                                  VL53L5CX_DetectionThresholds *thresholds)
{
  (void)dev;
  thresholds_calls++;
  memcpy(captured, thresholds, sizeof(captured));
  return thresholds_status;
}

uint8_t __wrap_vl53l5cx_set_detection_thresholds_enable(VL53L5CX_Configuration *dev,
                                                         uint8_t enabled)
{
  (void)dev;
  enable_calls++;
  last_enabled = enabled;
  return VL53L5CX_STATUS_OK;
}

static void check_resolution(uint8_t zones)
{
  VL53L5CX_Object_t sensor = {0};
  VL53L5CX_ITConfig_t config = {VL53L5CX_IT_IN_WINDOW, 100, 400};
  resolution = zones;
  thresholds_calls = 0;
  enable_calls = 0;

  assert(VL53L5CX_ConfigIT(&sensor, &config) == VL53L5CX_OK);
  assert(thresholds_calls == 1 && enable_calls == 1 && last_enabled == 1);
  for (uint8_t i = 0; i < zones; ++i)
  {
    uint8_t expected_zone = i;
    if (i == zones - 1U)
    {
      expected_zone |= VL53L5CX_LAST_THRESHOLD;
    }
    assert(captured[i].zone_num == expected_zone);
    assert(captured[i].measurement == VL53L5CX_DISTANCE_MM);
    assert(captured[i].type == VL53L5CX_IN_WINDOW);
    assert(captured[i].param_low_thresh == 100);
    assert(captured[i].param_high_thresh == 400);
  }
}

int main(void)
{
  VL53L5CX_Object_t sensor = {0};
  VL53L5CX_ITConfig_t config = {VL53L5CX_IT_IN_WINDOW, 100, 400};

  assert(VL53L5CX_ConfigIT(NULL, &config) == VL53L5CX_INVALID_PARAM);
  assert(VL53L5CX_ConfigIT(&sensor, NULL) == VL53L5CX_INVALID_PARAM);
  check_resolution(64);
  check_resolution(16);

  thresholds_calls = 0;
  enable_calls = 0;
  resolution = 0;
  assert(VL53L5CX_ConfigIT(&sensor, &config) == VL53L5CX_ERROR);
  resolution = 65;
  assert(VL53L5CX_ConfigIT(&sensor, &config) == VL53L5CX_ERROR);
  resolution = 64;
  resolution_status = 1;
  assert(VL53L5CX_ConfigIT(&sensor, &config) == VL53L5CX_ERROR);
  assert(thresholds_calls == 0 && enable_calls == 0);

  resolution_status = 0;
  thresholds_status = 1;
  assert(VL53L5CX_ConfigIT(&sensor, &config) == VL53L5CX_ERROR);
  thresholds_status = 0;
  config.Criteria = VL53L5CX_IT_DEFAULT;
  assert(VL53L5CX_ConfigIT(&sensor, &config) == VL53L5CX_OK);
  assert(last_enabled == 0);
  return 0;
}
