# MCU firmware images
This directory hosts firmware images for the actinius icarus v1.4 board with a stacked featherwing sdcard datalogger.
They can be uploaded to the Actinius icarus board by using for example:
```
mcumgr --conntype="serial" --connstring="dev=/dev/ttyUSB0,baud=115200" image upload imagefilename.bin
```
Not that the board needs to be in mcuboot mode though. This can be done by 
1. Press and hold the reset button
2. Press and hold the user button
3. Release reset button
4. Release user button


## Images
### Image to update modem firmware to version 1.2.7
`fmfu_smp_svr_icarusv1.4_app_update.bin` 

sha256 checksum: d963380293f777daf70deaed7202c09468dc83f19b13174a7069b122ace42cd6

This image can be uploaded to the board using `mcumgr` and it will update the modem firmware to version 1.2.7

### Version 1.0 Image to run GNSS-R logging application
`gnssrv1.0_icarusv1.4_mfw1.2.7_app_update.bin` 

sha256 checksum: 1b4f3651abf8d975b7cedcad0ca83cbb2ae26f853780bfb74bf6f621fbe18fc0

* GPS assisted GPS enabled (GPS-SUPL)
* External sim enabled
* Daily log uploading to webserver enabled


### Version 1.1 Image to run GNSS-R logging application
`gnssrv1.1_icarusv1.4_mfw1.2.7_app_update.bin` 

sha256 checksum:cab976c839cd79f9b643d002faa5366a1a27b5b60e5a0ebdf3954efd5af1a5af


* GPS assisted GPS enabled (GPS-SUPL)
* External sim enabled
* Daily log uploading to webserver enabled
