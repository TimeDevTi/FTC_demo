# Hardware / wiring notes

## Board pins and capacity

ข้อมูลอ้างอิงจาก [UNO Q hardware page](https://docs.arduino.cc/hardware/uno-q), [UNO Q datasheet](https://docs.arduino.cc/resources/datasheets/ABX00162-datasheet.pdf), [UNO Q user manual](https://docs.arduino.cc/tutorials/uno-q/user-manual/), [UNO R4 Minima datasheet](https://docs.arduino.cc/resources/datasheets/ABX00080-datasheet.pdf), และ [PlatformIO R4 Minima board](https://docs.platformio.org/en/latest/boards/renesas-ra/uno_r4_minima.html)

| Board | Relevant exposed I/O | Proposed use |
|---|---|---|
| UNO Q | STM32U585 owns 47 MCU GPIO; 22 digital exposed on UNO-style headers; six PWM and six analog-capable pins. Classic header IO is a 3.3 V domain; follow board pinout/5 V tolerance rules for each pin. | Linux MPU runs the UI. MCU UART `Serial1` on D0/RX, D1/TX connects to R4. MCU-to-Linux uses Arduino RouterBridge, not the header UART. |
| UNO R4 Minima | 14 digital header pins + A0–A5; six PWM pins D3/D5/D6/D9/D10/D11; A0–A5 can also be used as digital GPIO. | Four PWM + four DIR lines for four motor channels; D0/D1 are `Serial1` to UNO Q. D2, D11–D13 and A0–A5 remain spare. |

R4 use is **10 digital header pins total**: UART D0/D1 + four PWM + four DIR, of which D3/D5/D6/D9 are PWM. There are no pin conflicts in the proposed mapping. The four PWM signals come from four of six available PWM outputs; D10 is configured as DIR, despite also supporting PWM.

UNO Q MCU pin numbers beyond the physical UNO headers are not needed in this design. Use the board’s actual published pinout for revisions/connector variants rather than assuming all 47 STM32 GPIO are accessible through the Arduino API or UNO header.

## Proposed R4 Minima wiring

| Wheel | Controller (assumption) | R4 PWM | R4 DIR | Inverted |
|---|---|---:|---:|---|
| Front-left (FL) | Dual-channel driver channel 1 | D3 | D4 | no, adjust after lifted-wheel check |
| Front-right (FR) | Dual-channel driver channel 2 | D5 | D7 | no, adjust after lifted-wheel check |
| Rear-left (RL) | Single-channel driver 1 | D6 | D8 | no, adjust after lifted-wheel check |
| Rear-right (RR) | Single-channel driver 2 | D9 | D10 | no, adjust after lifted-wheel check |
| UNO Q UART RX/TX | R4 `Serial1` | D0 RX / D1 TX | — | see level shifting note |

Map `PWM` and `DIR` according to each driver’s actual labels and operating mode; common Cytron single-channel parts such as MD10C and dual-channel parts such as MDD10A support sign-magnitude PWM/DIR, but the user has not yet identified the exact three driver models. The PWM/DIR assumption must not be treated as verified wiring. Also confirm which channel is motor A/B and the physical location/motor polarity.

The board link is full-duplex UART:

```text
UNO Q D1 / Serial1 TX ---- level shifter ----> R4 D0 / Serial1 RX
UNO Q D0 / Serial1 RX <--- level shifter ----- R4 D1 / Serial1 TX
UNO Q GND ----------------------------------- R4 GND
```

Both boards’ IO are not to be assumed to use the same voltage domain. The UNO Q headers are 3.3 V MCU IO; the R4 Minima uses a 5 V-compatible UNO-style IO domain. Use a suitable two-channel UART level shifter for both data directions and a common ground. Do not connect either board’s 5 V power output to the other board’s 3.3 V pin. Driver logic input compatibility must also be checked against the driver manual.

## Power and safety before connecting motors

- The exact driver models and wheel motor specifications are not available yet (no photo file accompanied the request in the project folder). Read the full Cytron model/revision from all three boards; record motor rated voltage and stall current. Confirm each channel’s continuous/peak current and battery range before energizing.
- Power the motors from a correctly sized motor battery and Cytron driver supply. Do not power motors from an Arduino I/O pin or 5 V rail. Tie driver signal GND to R4 GND as the manufacturer specifies.
- Start by powering only the logic, then test with wheels lifted. Verify forward, reverse, strafe, and turn individually at low power; flip the corresponding `inverted` value in `include/robot/Config.h` if a wheel runs the wrong way.
- Add an accessible physical battery disconnect. The web STOP button and 250 ms data watchdog are not a certified emergency stop and cannot stop a failed/shorted driver.
- Never hot-plug motor, battery, or UART wires while powered.

## Watchdogs and behavior

- Browser sends drive updates on input changes and a 100 ms refresh while active; server lease expires after 250 ms without a browser command and asks R4 for zero output.
- R4 independently stops all PWM if a valid UART command is older than 250 ms or deadman is released.
- The UDP/Wi-Fi link is not in the motor control loop: Q MCU and R4 exchange local UART commands. Losing the browser-to-UNO-Q connection therefore expires the same drive lease and stops the motors.
- Web server is intentionally HTTP and binds all interfaces. Use a password-protected isolated AP only. Do not expose the unauthenticated control endpoint to a public/shared network.

## Web hotspot

`uno_q/start_hotspot.sh` uses `nmcli device wifi hotspot` (NetworkManager) with WPA2 password. It is a user-invoked setup helper, not an automatic boot service. Verify the UNO Q image’s NetworkManager and wireless driver support AP mode before relying on it; some board images or Wi-Fi driver versions may require their documented AP configuration. Set `WIFI_PASSWORD` securely and optionally `WIFI_SSID`/`WIFI_INTERFACE`; keep credentials out of source control. If the Q is connected to an existing router as a station instead, clients must share that LAN and use the UNO Q’s assigned IP.

## PlatformIO constraints

R4 Minima has official PlatformIO board ID `uno_r4_minima`. UNO Q’s Zephyr MCU is not yet supported by upstream PlatformIO; this project pins the community implementation `lee-lab-skku/arduinoq@1.0.0-rc.4`. Its board-specific workflow is Linux-only locally (x86_64/aarch64) and uses the UNO Q MPU/OpenOCD to upload, so keep R4 and Q upload instructions separate. This is not an Arduino/PlatformIO-endorsed platform.
