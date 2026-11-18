# ESP32 RC plane control link

This is the radio-control bench build for Danish Fernandes’s ATU Year 2 IoT project. An ESP32-C6 handheld transmitter reads two two-axis joysticks and four toggles. An ESP32-WROOM-32D receiver on the aircraft receives those controls, displays them and drives the ESC throttle output.

The project is intended for a Sonicmodell AR Wing Classic 900 mm. The current firmware implements the wireless control link and bench diagnostics. The earlier BMX055 compass/IMU and the planned BMP388 barometer are outside these sketches. Attitude estimation, stabilisation and the AR Wing’s two-servo elevon mixing are future work. The receiver includes optional independent servo PWM outputs for a conventional roll/pitch/yaw layout, disabled by default.

## Install and upload

Use Arduino IDE with **esp32 by Espressif Systems 3.x**, **Adafruit GFX Library** and **Adafruit SSD1306**. The sketches use the ESP32 core’s Wi-Fi, Wire, WebServer, Preferences and LEDC APIs. Keep each sketch folder intact so Arduino can find its headers.

| Sketch | Target | Typical generic Arduino board selection |
|---|---|---|
| [Handheld.ino](Handheld/Handheld.ino) | ESP32-C6 handheld | ESP32C6 Dev Module |
| [FlightController.ino](FlightController/FlightController.ino) | Classic ESP32-WROOM-32D receiver | ESP32 Dev Module |

Upload both sketches as a pair. They use version 2 of the control packet. The older throttle-only firmware uses version 1.

## Wiring

GPIO numbers refer to pin labels on each dev board. Both OLEDs are configured as 128×64 I²C SSD1306 displays. The sketches probe addresses `0x3C` and `0x3D` at startup.

### ESP32-C6 handheld

| Device | Signal | GPIO |
|---|---|---:|
| Left stick X | Yaw | 2 |
| Left stick Y | Throttle, inverted in software | 3 |
| Right stick X | Roll, inverted in software | 0 |
| Right stick Y | Pitch | 1 |
| Toggle S1 | Switch input | 21 |
| Toggle S2 | ESC arm/disarm request | 20 |
| Toggle S3 | Switch input | 19 |
| Toggle S4 | Switch input | 18 |
| OLED | SDA | 6 |
| OLED | SCL | 7 |
| Onboard RGB LED | Data | Board definition’s `RGB_BUILTIN`, with GPIO8 as fallback |

Power both sticks and the OLED from 3V3 and GND. Each toggle connects its GPIO to GND when closed. The internal pull-up holds an open switch HIGH. A closed switch reads LOW and is shown as ON or 1.

### ESP32-WROOM-32D flight controller

| Device | Signal | GPIO |
|---|---|---:|
| OLED | SDA | 21 |
| OLED | SCL | 22 |
| ESC | PWM signal | 25 |
| Optional roll servo | PWM signal | 13 |
| Optional pitch servo | PWM signal | 14 |
| Optional yaw servo | PWM signal | 32 |

The OLED connects to 3V3 and GND. The ESC signal ground connects to ESP32 GND. During USB-powered testing, leave the ESC’s BEC positive lead, if present, disconnected and insulated. If testing servos, use a supply rated for them and join its ground to ESP32 GND. Servo pins in the table are signal outputs.

## Calibrate the handheld

1. Leave S2 off. Connect a phone or laptop to **Stick-Cal** with password **`sticksetup`** and open [192.168.4.1](http://192.168.4.1/). The handheld hosts the network and web page. The receiver joins it directly.
2. Tap **Start calibration**. Sweep both sticks through all four corners, then tap **Record limits**.
3. Release the spring-centred axes and hold the throttle halfway. Tap **Capture centre** and hold steady for one second.
4. With S2 still off, check the web page reports 0% with throttle fully down and 100% fully up. If the throttle direction is reversed, check the second entry in `AXIS_REVERSED` in `StickInputs.h`. Recalibrate after changing the physical axis mapping.
5. Adjust **Centre deadzone** if the crosshair jitters. The range is 0–20%, with 3% as the initial setting. The slider previews changes and saves the final value on release. **Cancel** keeps the previous calibration.

The handheld averages eight 12-bit ADC samples per axis. It records a minimum, centre and maximum for each axis and validates at least 60 counts between the centre and each endpoint. It stores valid calibration in flash using Preferences namespace `stick-cal`, key `axes-v1`. The deadzone uses the same namespace and key `deadzone`. If a valid calibration hasn’t been saved, the firmware can display and transmit readings using 0/2048/4095 as defaults. The receiver displays **CAL NEEDED** and keeps the ESC at minimum.

Yaw, roll and pitch use a centred range of **−1000 to +1000**. The code applies the deadzone separately on each side of centre and rescales the rest of the travel. The throttle channel uses **0 to 1000**, representing 0–100% in tenths of a percent, with a fixed 3% band at the low endpoint. The handheld OLED plots the throttle stick around its calibrated midpoint like the other stick axes. Use the webpage or receiver display for actual throttle percent.

## Bench test the link and outputs

1. Start with the **ESC battery disconnected**, both ESP32s powered by USB and S2 off. The receiver OLED should show **WAITING FOR LINK** until valid packets arrive, then **LINK OK** or **LINK OK / CAL NEEDED**. The receiver Serial Monitor reports the same information at **115200 baud**.
2. Move one axis at a time and flip each toggle separately. The receiver OLED labels are `Y` for yaw, `T` for throttle, `R` for roll and `P` for pitch. S1, S3 and S4 are displayed for diagnostics and have no output assignments.
3. After calibration, hold throttle fully down with S2 off for at least **500 ms**, then flip S2 on at idle. The handheld requires **40 ms** of stable S2 ON. Confirm **ARMED** on the receiver, then raise the stick and check the displayed pulse width increases. The number on the OLED is the commanded pulse width. A scope or logic analyser can measure the actual signal on GPIO25.
4. Switch S2 off and check the receiver disarms. Repeat with the transmitter powered off to check link loss. After loss of valid packets for **250 ms**, or a detected Wi-Fi disconnection, the code clears the received controls and commands minimum throttle. With the link restored, cycle S2 off at idle and repeat the arming sequence.
5. For a powered ESC test, remove the propeller and secure the motor. Check the ESC’s input specification against the configured **50 Hz, 1000–2000 µs** range and the ESP32’s 3.3 V signal before connecting motor power.

The receiver considers a throttle value at or below **20/1000** idle. At idle and when disarmed, it commands 1000 µs. Above idle while armed, it scales the received throttle into the configured range. Starting a new calibration clears the packet’s calibration flag and disarms the receiver.

The optional servo module is disabled in `ServoOutputs.h`. When enabled, it generates 50 Hz pulses with a 1500 µs centre and ±400 µs travel. It centres the outputs on link loss or while the calibration flag is clear. S2 gates the ESC only. A future AR Wing setup needs a mixer that combines pitch and roll into separate left and right elevon commands.

## How the code is organised

| File | Job |
|---|---|
| `Handheld/Handheld.ino` | Starts inputs and the display, loads flash settings, starts Wi-Fi and runs the input, display and web loops. |
| `Handheld/StickInputs.h` | Defines joystick and switch pins, reads ADC samples, applies inversion and deadzones, and manages calibration and flash storage. |
| `Handheld/OledDisplay.h` | Draws both stick plots, toggle states and calibration prompts. |
| `Handheld/CalibrationWeb.h` | Serves the page at `/`, supplies live status at `GET /state`, and handles `POST /calibrate` and `POST /deadzone`. |
| `Handheld/StatusLed.h` | Red indicates AP or calibration unavailable. Green indicates ready with S2 off. Blue indicates a local S2 enable request. Amber, purple and flashing white indicate the calibration stages. Confirm actual arming on the receiver. |
| `Handheld/RadioLink.h` | Starts the Wi-Fi AP on channel 6, builds a packet from the latest controls and sends it over UDP. |
| `FlightController/FlightController.ino` | Starts PWM, OLED and radio, then polls the link, updates optional servos and reports status. |
| `FlightController/RadioLink.h` | Joins the AP, verifies packets, stores the latest decoded controls and triggers disarm on link loss. |
| `FlightController/OledDisplay.h` | Displays link status, armed status, pulse width, four axes and four switches. |
| `FlightController/EscOutput.h` | Owns GPIO25 PWM, the S2 arming sequence and throttle scaling. |
| `FlightController/ServoOutputs.h` | Owns optional roll, pitch and yaw PWM, including neutral and reversal settings. |
| `ControlProtocol.h` in both folders | Defines the same frame, Wi-Fi credentials, UDP port and byte encoding. Change both copies together if changing the protocol. |

The handheld aims to read and send controls every **20 ms** and refresh its OLED every **50 ms**. Its calibration page polls status every **400 ms**. The flight controller runs its radio polling in the main loop, refreshes its OLED every **100 ms** and reports to Serial every **500 ms**.

## Packet reference

The handheld sends a **13-byte UDP datagram** from `192.168.4.1` to the receiver’s static IP, `192.168.4.200`, on port **4210**. The link is one-way. The receiver checks sender IP, length, signature, version, flags and axis ranges. The Wi-Fi link is a prototype and the packet has no sequence number, acknowledgement or application-level authentication.

| Byte offset | Contents |
|---|---|
| 0–1 | ASCII signature `ST` |
| 2 | Protocol version, currently 2 |
| 3 | Raw toggle states, bits 0–3 for S1 through S4 |
| 4 | Bit 0 means stored calibration is valid and the handheld is in display mode. Bit 1 means S2 is stably ON. |
| 5–6 | Yaw plus 1000 |
| 7–8 | Throttle, 0–1000 |
| 9–10 | Roll plus 1000 |
| 11–12 | Pitch plus 1000 |

All four axis values use two bytes with the most significant byte first. The receiver subtracts 1000 from the centred axes after decoding.

## Troubleshooting

| Symptom | Check |
|---|---|
| Receiver OLED stays blank | Check 3V3, GND, GPIO21 SDA and GPIO22 SCL. The receiver logs whether it found the OLED at 115200 baud. The OLED should display a message before the receiver joins Wi-Fi. |
| Receiver remains on WAITING FOR LINK | Confirm `Stick-Cal` is visible and both boards have the matching version 2 sketches. |
| Link works but ESC stays disarmed | Check for **CAL NEEDED**, verify throttle reaches 0%, then repeat the S2-off idle hold and switch on at idle. |
| Wrong toggle number on the receiver | Check the handheld order in `SWITCH_PINS`. S1–S4 are GPIO21, 20, 19 and 18. |
| Stick dot jitters at rest | Check centre calibration and increase the centre deadzone a little. |
| Servos show no movement | Check whether the optional output module was enabled and wired for the intended bench setup. |

The previous compile attempt stopped when Windows refused to start the two ESP32 compiler executables. Compilation and hardware behaviour for this revision remain unverified.
