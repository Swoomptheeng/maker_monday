# The Swoompipe
Arduino files containing code relating to the Swoompipe. Originally a BCU Maker Monday Commission.
More on the project at http://swoomptheengs.tumblr.com

The 'Swoompipe' is a weird MIDI bagpipe. It's squeezable (via Flex Sensor), blow-into-able (via Electret Mic), tilt and motion sensing (via Accelerometer/Gyroscope) and playable with 12 capacitive touch sensitive keys. 

It's being developed by Ben Neal (www.psiconlab.co.uk) with Swoomptheeng (www.Swoomptheeng.com) originally as part of a Birmingham City University Maker Monday Commission and was shown at the Victoria and Albert Museum, London as part of Digital Design Weekend 2017 thanks to BOM (Birmingham Open Media).


v1. Uses flex sensor, electret mic & capacitive Keys over USB

v2. Uses adds an accelerometer/gyroscope

v3. Swaps USB for Bluetooth communications and loses the flex sensor


This system connects to a Windows laptop and uses the Hairless MIDI Serial (http://projectgus.github.io/hairless-midiserial/) to capture serial data over USB or Bluetooth COM ports from the Arduino and sends it to the BeLoop virtual MIDI driver (http://www.nerds.de/en/loopbe1.html) which controls music software and games via the MIDI protocol.

<img src="http://swoomptheeng.com/wp-content/uploads/png/SwoomptheengSwoompipe.png" />
The v3 Swoompipe (just the pipe section)
