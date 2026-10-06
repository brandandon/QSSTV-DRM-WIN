QSSTV for Windows - setup tips
==============================

QSSTV sends and receives digital SSTV (HamDRM, compatible with EasyPal
and QSSTV on Linux) as well as normal analog SSTV.


1. If Windows shows "Windows protected your PC"
-----------------------------------------------
The installer is not code-signed, so Windows SmartScreen may warn you.
Click "More info", then "Run anyway".


2. Give your radio's sound device a clear name (recommended)
------------------------------------------------------------
Radios with a built-in USB sound card (Yaesu FT-991A, Icom IC-9700 and
many others) all show up in Windows as "USB Audio CODEC". Renaming them
makes them easy to pick and stops mix-ups if you have more than one radio.

  a. Right-click the speaker icon by the clock > Sound settings >
     More sound settings.
  b. Playback tab: double-click the radio's "USB Audio CODEC" speaker,
     type a new name such as "FT-991A TX", click OK.
  c. Recording tab: do the same for its microphone, e.g. "FT-991A RX".
  d. While you are there, on the Advanced tab of each one, set
     "16 bit, 48000 Hz" and untick any "Enable audio enhancements".


3. Pick the sound devices in QSSTV
----------------------------------
Options > Configuration > Sound: choose the radio's devices for Input
and Output. If the radio was renamed after QSSTV was opened, restart
QSSTV so the new names show up.


4. Let QSSTV key the radio (PTT)
--------------------------------
Options > Configuration > CAT: tick "Enable Hamlib Cat Interface",
choose your Radio Model, enter the radio's COM port (e.g. COM5, see
Device Manager > Ports) as the Serial Port, and the Baudrate set in the
radio's menu. PTT is then sent over the USB cable, no
VOX needed.


5. Radio settings
-----------------
  - Use the radio's DATA mode (DATA-U / USB-D) on HF.
  - Set the radio's USB/data input level so the ALC barely moves.
    Overdriving distorts digital SSTV and other stations will not
    decode it.


6. Pictures in the waterfall
----------------------------
  - "WF Image" button (bottom right): pick a picture to send in the
    waterfall. It is sent about 100 pixels wide and up to about 10
    seconds long. Simple, high-contrast pictures look best.
  - To send a logo automatically with every image, enter
        img:C:\path\to\logo.png
    in "Start Picture" (or "End Picture") under
    Options > Configuration > Waterfall.


Your images are kept in Documents\qsstv.
