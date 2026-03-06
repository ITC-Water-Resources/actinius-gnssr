#!/usr/bin/bash




url=$1

echo $url


openssl s_client -showcerts -connect ${url}:443 </dev/null > ${url}.certficates



#extract certificates with line endingd repaced with '$' character 
awk 'BEGIN{n=0;doprint=0}/BEGIN CERTIFICATE/{doprint=1;print "\nCertificate",n;n++}doprint==1{sub(/$/,"$");printf("%s",$0)}/END CERTIFICATE/{doprint=0}' ${url}.certficates



echo "for pem includes:"

awk 'BEGIN{n=0;doprint=0}/BEGIN CERTIFICATE/{doprint=1;print "\nCertificate",n;n++}doprint==1{sub(/$/,"\\n");printf("\x22%s\x22\n",$0)}/END CERTIFICATE/{doprint=0}' ${url}.certficates
