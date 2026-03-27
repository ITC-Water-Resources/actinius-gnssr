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

### Version 1.3.1 Image to run GNSS-R logging application (27 March 2026)
`gnssrv1.3.1_icarusv1.4_mfw1.2.7_app_update.bin` 

sha256 checksum: 50e0ca8f1d7b25cb59fef559c0345e7ab588fb169424d3c05727483e82dc0e25


* Fix initialize last_http_status for every file upload 

### Version 1.2 Image to run GNSS-R logging application (March 2026)
`gnssrv1.2_icarusv1.4_mfw1.2.7_app_update.bin` 

sha256 checksum: f3dd9b42a2fbb819c6f3d4c8b504c7271f2337bf96c88394aab18421e3896641


* Rebuild with nrf v1.5.2 (still supported for the last mode version 1.2.7 supported on nrf9160B0 )
* Fix TLS connection errors with new surfdrive.surf.nl server
* This is (hopefully) the last image for the (legacy) Actinius v1.4 board

### Version 1.1 Image to run GNSS-R logging application
`gnssrv1.1_icarusv1.4_mfw1.2.7_app_update.bin` 

sha256 checksum:cab976c839cd79f9b643d002faa5366a1a27b5b60e5a0ebdf3954efd5af1a5af


* GPS assisted GPS enabled (GPS-SUPL)
* External sim enabled
* Daily log uploading to webserver enabled

### Version 1.0 Image to run GNSS-R logging application
`gnssrv1.0_icarusv1.4_mfw1.2.7_app_update.bin` 

sha256 checksum: 1b4f3651abf8d975b7cedcad0ca83cbb2ae26f853780bfb74bf6f621fbe18fc0

* GPS assisted GPS enabled (GPS-SUPL)
* External sim enabled
* Daily log uploading to webserver enabled



