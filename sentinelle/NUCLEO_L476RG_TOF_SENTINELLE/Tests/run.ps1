$ErrorActionPreference = 'Stop'
$prefix = Join-Path ([System.IO.Path]::GetTempPath()) ("vl53l5-tests-" + [guid]::NewGuid())
$busTest = "$prefix-bus.exe"
$thresholdTest = "$prefix-thresholds.exe"

Push-Location (Join-Path $PSScriptRoot '..')
try {
  & gcc -std=c11 -Wall -Wextra -Werror '-ITests\mocks' '-IInc' `
    'Tests\test_i2c_bus.c' 'Src\stm32l4xx_nucleo_bus.c' -o $busTest
  if ($LASTEXITCODE -ne 0) { throw 'I2C test compilation failed.' }
  & $busTest
  if ($LASTEXITCODE -ne 0) { throw 'I2C test failed.' }

  & gcc -std=c11 -Wall -Wextra `
    '-IDrivers\BSP\Components\vl53l5cx' `
    '-IDrivers\BSP\Components\vl53l5cx\modules' `
    '-IDrivers\BSP\Components\vl53l5cx\porting' `
    'Tests\test_vl53l5cx_thresholds.c' `
    'Drivers\BSP\Components\vl53l5cx\vl53l5cx.c' `
    'Drivers\BSP\Components\vl53l5cx\modules\vl53l5cx_api.c' `
    'Drivers\BSP\Components\vl53l5cx\modules\vl53l5cx_plugin_detection_thresholds.c' `
    'Drivers\BSP\Components\vl53l5cx\modules\vl53l5cx_plugin_xtalk.c' `
    'Drivers\BSP\Components\vl53l5cx\porting\platform.c' `
    '-Wl,--wrap=vl53l5cx_get_resolution' `
    '-Wl,--wrap=vl53l5cx_set_detection_thresholds' `
    '-Wl,--wrap=vl53l5cx_set_detection_thresholds_enable' -o $thresholdTest
  if ($LASTEXITCODE -ne 0) { throw 'Threshold test compilation failed.' }
  & $thresholdTest
  if ($LASTEXITCODE -ne 0) { throw 'Threshold test failed.' }
  Write-Output 'I2C bus and sensor threshold tests passed.'
}
finally {
  foreach ($test in @($busTest, $thresholdTest)) {
    if (Test-Path -LiteralPath $test) { Remove-Item -LiteralPath $test }
  }
  Pop-Location
}
