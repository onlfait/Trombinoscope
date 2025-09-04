\# Trombinoscope LED Selector



A \*\*random people selector\*\* using a single button and a NeoPixel strip/panel.  

Designed for classrooms, workshops, or fun group activities — assign tasks randomly, from sweeping the floor to presenting first!



---



\## Features

\- \*\*One button only\*\*:

&nbsp; - 1 short press → 1 person selected  

&nbsp; - 3 short presses → 3 people selected  

&nbsp; - Long press → reset / cancel  

\- \*\*Rainbow idle animation\*\* while waiting

\- \*\*Flashy transition animation\*\* before selection

\- \*\*Unique random selection\*\* (no duplicates in the same round)

\- Configurable timings \& number of people

\- Works with any WS2812/WS2813 strip or panel



---



\## Hardware

\- Arduino (Uno, Nano, ESP32, etc.)

\- WS2812/WS2813 NeoPixel LED strip/panel

\- 1 push button wired to ground + `BUTTON\_PIN` (with `INPUT\_PULLUP`)



\### Wiring

| Component     | Arduino Pin |

|---------------|-------------|

| LED strip DIN | 15          |

| Button        | 16 (to GND) |

| LED strip VCC | 5V          |

| LED strip GND | GND         |



> Adapt pins in the code if you use another setup.



---



\## How it works

1\. \*\*Idle\*\* → Rainbow animation cycles through all people LEDs.  

2\. \*\*Counting\*\* → Each short press increments the number of people requested.  

&nbsp;  - If no press after `COUNT\_WINDOW\_MS` (default 900 ms), the count is frozen.  

3\. \*\*Animation\*\* → A colorful “storm” plays for `ANIM\_MS` (default 1.8 s).  

4\. \*\*Show\*\* → Randomly pick `K` unique people and light them in \*\*white\*\*.  

&nbsp;  - Display lasts `HOLD\_MS` (default 10 s).  

5\. Return to \*\*Idle\*\* and repeat.



---



\## Configuration

All settings are in the top of `trombinoscope.ino`:



```cpp

\#define LED\_PIN        15

\#define NUM\_LEDS       30

\#define BUTTON\_PIN     16



// People LEDs (indices on the strip)

const uint16\_t UsedLEDs\[] = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20};



