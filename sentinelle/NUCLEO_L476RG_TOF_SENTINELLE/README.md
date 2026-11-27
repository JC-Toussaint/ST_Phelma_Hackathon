# NUCLEO-L476RG with X-NUCLEO-53L5A1

Connect a X-NUCLEO-53L5A1 expansion board to the Arduino headers of a NUCLEO-L476RG board.

Import this directory as an existing STM32CubeIDE project,
build the Debug or Release configuration, and flash it through the board's
ST-LINK. The project includes a template NanoEdgeAI classification library.

Connect a serial terminal to the ST-LINK virtual COM port at **115200 baud,
8 data bits, no parity, 1 stop bit**, with no flow control. In the default data
logging mode, a target within 100-400 mm triggers a line of 512 space-separated
distance values. Values outside that window are reported as `0.0`. Each line
contains eight consecutive 8x8 measurements at 15 Hz.

## Application settings

Edit `X-CUBE-TOF1/App/app_x-cube-tof1.c`:

- `NEAI_MODE`: `0` logs data; `1` runs the classifier and prints the class name.
- `MATRIX_RESOLUTION`: `64` for 8x8 or `16` for 4x4.
- `SUCCESSIVE_MATRIX`: a power of two from 1 to 64.
- `MIN_THRESHOLD` and `MAX_THRESHOLD`: the detection window in millimetres,
  from 40 to 4000 mm.

Classification requires the right buffer size to match the model in
`Inc/NanoEdgeAI.h`; the template library expects 512 distance values. When
deploying a different NanoEdgeAI library, replace both that header and
`Src/libneai.a`.
