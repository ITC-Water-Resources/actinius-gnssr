# actinius-gnssr
This repository will host the code and instructions to to build a low cost GNSS reflectometer based on the actinius-icarus board (Nordic nrf9160 chip)
together with an Adafruit featherwing data logger

# Current Status: under construction ( nothing is working yet) 

## TODO Sofware:
1. ~~Setup communication with the sdcard from the data logger (uses SPI3 protocol)~~
2. Setup the setting of the clock on the datalogger (uses a serial connection on i2c of the feather board). possibly use GPS nmea messages to set the time (every once in a while?)
3. ~~Allow writing of lz4 compressed files to the sdcard (to store nmea messages, and telemetry)~~
4. Add functionality to upload nmea logs to an external server using the on board LTE chip
5. (Bonus add functionality to to a remote over-the air update of the firmware)
6. Read privacy sensitive settings from configuration file (such as upload passwords etc.)

## TODO:Hardware
1. Assemble battery, charger and solar panel, and case

# Steps for building the firmware (command line version)
## Setting up the software development kit and installing a dedicated python virtual environment
Building this firmware requires [installing the Software development Kit from Nordic](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/getting_started.html). Which is an extended version of the Zephyr development kit. For the discussion here, it is assumed that the development kit is installed in he user's home under ncs `${HOME]/ncs`.

The current stack was build by setting up a dedicated python environment [using the command line interface to the zephyr buidl system](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/gs_installing.html#set-up-the-command-line-build-environment)
Once a new python environment is created `python -m venv pynrf` in the users home, one can add the following to the bottom of ${HOME}/pynrf/bin/activate
```


## added by RR
source ${VIRTUAL_ENV}/nrfenvs
```
The file `nrfsenvs` should be constructed in `${HOME}/pynrf/bin` and contains instructions to find the zephyr-sdk environment and the toolchain:
```
source /home/roelof/ncs/zephyr/zephyr-env.sh
```

## Building the firmware 
From a terminal which has loaded the `pynrf` environment, change to the `firmware_src` directory of this repository and execute
`west build -b actinius_icarus_ns -d build`
or 

`west build -b actinius_icarus_ns -d build --pristine` When all the filed in the build directory needs to be overwritten
If all goes well, this should create a new firmware image `firmware_src/build/zephyr/app_update.bin` 

# Uploading firmware using mcumgr
You can now upload the firmware to the actinius board using `mcumgr`
`mcumgr --conntype="serial" --connstring="dev=/dev/ttyUSB0,baud=115200" image upload build/zephyr/app_update.bin`


# checking the uart serial output on linux
There is a [convenience script](debugtools/catserial.sh), which sets the correct serial baud rate etc and spits out the serial output from the Icarus.

The correct settings are:
- baud rate: 115200
- data bits:8
- stop bit: 1
- parity: None
- Flow control: None


#Device info:
Application version: 1.4.4
Modem firmware version: mfw_nrf9160_1.2.3
[nrf Buildchain used in this version] (https://developer.nordicsemi.com/nRF_Connect_SDK/doc/1.5.1/nrf/index.html) 
