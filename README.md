# Best-cookie-clicker-ever: cookie Clicker with a Gambling Mode

This is a small handheld game pad built around the Seeed XIAO RP2040 and a 1.8" 128x160 TFT screen. It runs a cookie clicker game, tap keys to earn clicks, unlock a bonus key at 5,000 clicks, and gamble your clicks on a 50/50 double or nothing roll, good luck :3 It is a custom PCB designed in KiCad

<img width="1364" height="1032" alt="image" src="https://github.com/user-attachments/assets/9dafa451-fc2f-4885-bbb7-a0e5f9fd3782" />
And I wrote hackclub all over it because hackclub is goated 

## Why I Made This
There are online cookie clickers. But I think I real one needs actual mechanical keys that look pretty and its own screen. So with a display, the first thing that came to my mind is I want to make a keypad that displays the amount of clicks. 
Also who doesn't like gambling
I am making this for my friend who likes geometry dash so he probably likes clicking keys.

## How It Works

### Controls
<img width="1524" height="1536" alt="image" src="https://github.com/user-attachments/assets/4ae90793-601d-4868-83e8-4a563e26e789" />

| Key | Normal mode | Gamble mode |
|-----|-------------|-------------|
| SW1 (top left) | +1 click | +1 click |
| SW4 (top right) | Locked until you reach 5,000 clicks, then +2 clicks per press | Same |
| SW5 (middle left) | Enter gamble mode | Leave gamble mode |
| SW2 (middle right) | Nothing | Confirm the bet and roll |
| SW6 (bottom left) | Nothing | Bet +100 (hold to repeat) |
| SW3 (bottom right) | Nothing | Bet -100 (hold to repeat) |

Hold SW6 and SW3 together for 3 seconds to wipe the saved game.

### Game features

- **Click counter:** the screen shows your total clicks with thousands separators. Theres a progress bar fills toward the 5,000 click unlock.
- **Gambling:** the bet starts at 100 and moves in steps of 100, capped by how many clicks you have. A roll is a true 50/50 using the RP2040's hardware random number generator. A win doubles your bet, a loss loses it.
- **Fair saves:** the bet is taken out and saved to flash before the roll happens muahahahaha
- **Progress:** clicks, wins, losses and the bonus unlock are saved to flash and restored on power up.
- Random ASCII art (a cookie, a cat, a rocket, a star etc) pops up at random positions while you click. Gamble mode has its own dice, win and skull art.
  
### The circuit
<img width="1189" height="844" alt="image" src="https://github.com/user-attachments/assets/3c2dce54-2cd5-487d-8143-5d314c976b88" />


- **Microcontroller:** Seeed XIAO RP2040 (U1).
- **Keys:** each of the 6 switches connects one GPIO pin to GND. The firmware turns on the RP2040's internal pull-up resistors, so a pressed key reads LOW and no external resistors are needed. The firmware debounces each key for 15 ms.
- **Screen:** the 1.8" ST7735 TFT module connects through the 8-pin header J1 and talks over hardware SPI0 (SCK on GPIO2, MOSI on GPIO3). The screen is write only, so MISO is not used. That frees GPIO4, which is why the +100 key is on that pin, and the firmware explicitly releases it from SPI.
- **Power:** everything runs from USB-C through the XIAO. The screen is powered from the XIAO's 3.3V pin.

| Signal | XIAO pin | GPIO |
|--------|----------|------|
| TFT SCK | D8 | GPIO2 |
| TFT MOSI | D10 | GPIO3 |
| TFT CS | D7 | GPIO1 |
| TFT DC | D6 | GPIO0 |
| TFT RST | D3 | GPIO29 |
| SW1 (click) | D0 | GPIO26 |
| SW2 (confirm) | D1 | GPIO27 |
| SW3 (bet -100) | D2 | GPIO28 |
| SW4 (gold key) | D4 | GPIO6 |
| SW5 (gamble) | D5 | GPIO7 |
| SW6 (bet +100) | D9 | GPIO4 |

### The PCB

<img width="1198" height="991" alt="image" src="https://github.com/user-attachments/assets/ca818d3d-c257-4b67-9cfd-160f4a54229d" />


The board is a single PCB with the XIAO and screen header on the left, six hot swap key switches in a 2 by 3 grid on the right, and custom silkscreen art. I made this board small so it is more portable and I chose keys that is assembled directly on the pcb. 

## Bill of Materials

| Item | Qty | Supplier | Cost/Item (CAD) | Total Cost (CAD) | Link |
|------|-----|----------|-----------------|------------------|------|
| Seeed XIAO RP2040 | 1 | Seed studio | $4.9 | $4.9 | [[link]](seeedstudio.com/Seeed-Studio-XIAO-RP2040-Plus-p-6932.html?c_event_medium=search%3A%3Aresults&c_event_id=4057244488%3A%3ASeeed+XIAO+RP2040) |
| 1.8" 128x160 ST7735 TFT module (SPI, 8 pin) | 1 | Canada Robotics | $12.99 | $12.99 | [[link]](https://www.canadarobotix.com/products/3056?srsltid=AYCX9vLb8w1gLBkC4ExwkK73b0u5_ToArs0HsUIRm7UbcOfMoLzOpVDB) |
| Key switches | 6 | YUNZII | $12.99 | $12.99 | [[link]](https://www.canadacomputers.com/en/keyboard-switches/285141/yunzii-milk-v2-linear-switch-35-pieces-per-pack-milk-v2.html) |
| Keycaps | 6 | Canada computers | $24.99 | $24.99 | [[link]](https://www.canadacomputers.com/en/keycaps/281092/keychron-artisan-keycap-galaxy-resin-sa-keycaps-rt-32.html) |
| PCB fabrication (JLCPCB) | 1 | JLCPCB | $14.99 | $14.99 | [[link]](https://jlcpcb.com/) |
| | | | **Total** | **$70.86** | |


## Firmware Flashing Guide

The firmware is a single Arduino sketch:
### What you need

- Arduino IDE 2.x 
- A USB-C data cable
- The assembled pcb with the XIAO RP2040 and TFT module attached

### 1. Install the RP2040 board package

1. Open Arduino IDE and go to **File > Preferences**.
2. Go to **Tools > Board > Boards Manager**, search for `pico`, and install **Raspberry Pi Pico/RP2040** by Earle F. Philhower, III.

### 2. Install the libraries

Go to **Sketch > Include Library > Manage Libraries** and install:

- **Adafruit GFX Library**
- **Adafruit ST7735 and ST7789 Library**
install the dependencies

### 3. Open the sketch

Get the ino file in this repo, the sketch must stay inside a folder named `cookie_clicker`, or Arduino IDE will ask to move it.

### 4. Select the board

Go to **Tools > Board > Raspberry Pi Pico/RP2040 > Seeed XIAO RP2040**.

### 5. Put the XIAO in bootloader mode and upload

1. Hold the B button on the XIAO and plug it into your computer over USB-C. Release the button. A drive named `RPI-RP2` appears.
2. In Arduino IDE, select the new port under **Tools > Port**. If no port shows yet, you can still press Upload and the IDE will find the board.
3. Click the **Upload** button (right arrow, top left).
4. Wait for the upload to finish. The board reboots by itself and the game starts on the screen.

After the first upload, you can upload again without the boot button.

### Troubleshooting
If! you want to reset your save: hold SW6 and SW3 together for 3 seconds.
