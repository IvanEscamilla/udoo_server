#!/bin/bash
#Script para conectar red wifi
#

echo "Configurando Wifi"
echo "Introduce nombre de red: "
read SSID
echo "Tiene contraseña? [1 o 2]"
echo "1. Si"
echo "2. No"
read contra

if [ $contra -eq 1 ] ; then

	echo "1. WPA-PSK"
	echo "2. WPA-EAP"
    echo "Que tipo de seguridad? [1 o 2]"
    read tipo
    if [ $tipo -eq 1 ] ; then
        $ key_mgmt = WPA-PSK
    else
        $ key_mgmt = WPA-EAP
    fi
    
    echo "Introduce Password: "
    read pass

else
    
    $ key_mgmt = NONE
    
fi

wpa_cli -i wlan0
sleep 2;

for (( i=1; i<=5; i++ ))
do
    for (( j=1; j<=i;  j++ ))
    do
     echo -n "$i"
    done
    echo ""
done

#
# ./ch.sh: vivek-tech.com to nixcraft.com referance converted using this tool
# See the tool at http://www.nixcraft.com/uniqlinuxfeatures/tools/
#