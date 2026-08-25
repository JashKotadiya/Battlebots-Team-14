# Battlebots Team 14

### Full build documentation: https://jashkotadiya.github.io/Battlebots-Team-14/

A 3D-printed antweight combat robot built by Team 14 at the UMass Robotics Club, UMass Amherst.

The entire outer ring is the weapon. Gear teeth are printed around its inner wall and a pinion on
the brushless motor meshes into them, so the whole shell spins around a two-wheel drive base. The
site above covers the full build log with photos and video, the control system reference, the kit
cost breakdown, and the notes for the next revision.

## Firmware

`Battlebots.ino` is the build that drove the machine. It runs on an ESP32 and handles:

- Arcade drive: right trigger for throttle, left trigger for brake and reverse, left stick to steer
- Weapon on its own channel, held to a speed ceiling set in firmware
- Ramped acceleration with faster deceleration, on both the drive and the shell
- Latching emergency stop on the O button
- Automatic failsafe that neutralises drive and stops the shell if the controller drops

Three PWM channels run off the ESP32 LEDC peripheral at 14-bit resolution and 50 Hz, the standard
20 ms servo and ESC frame. Input comes from an Xbox controller over Bluetooth through the Bluepad32
library, so any gamepad that library supports will drive it without a code change.

## Hardware

| Part | Detail |
| --- | --- |
| Controller | ESP32, Bluepad32 |
| Drive | 2x brushed motors on GPIO 12 and 14, dual 2x2A brushed ESC |
| Weapon | Brushless outrunner on GPIO 18, Readytosky 35A BLHeli_S |
| Power | 4S HV LiPo, Gaoneng GNB 380 mAh, 15.2 V, 90C |
| Chassis | 3D-printed PLA, drive base plus toothed shell |

## Repository

| Path | Contents |
| --- | --- |
| `Battlebots.ino` | The firmware |
| `docs/` | The documentation site, published with GitHub Pages |
