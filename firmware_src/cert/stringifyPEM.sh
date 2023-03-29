#!/bin/bash
# R. Rietbroek, 2023
#make one big string out of a PEM certificate and replace the actual line enhds with '\n'
#This string can then be used to assign a tls certificate string to a json variable
#
awk '{gsub("\r","",$0);printf "\x22%s\\n\x22\n",$0}' $1
