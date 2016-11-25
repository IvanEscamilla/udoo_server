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
echo ""
if [ $contra -eq 1 ] ; then

    echo "Que tipo de seguridad? [1 o 2]"
	echo "1. WPA-PSK"
	echo "2. WPA-EAP"
    read tipo
    if [ $tipo -eq 1 ] ; then
        $key_mgmt = WPA-PSK
    else
        $key_mgmt = WPA-EAP
    fi
    
    echo "Introduce Password: "
    read pass

    output=$(nmcli device wifi connect "$SSID" password "$pass" iface wlan0 --timeout 10)
else
    $key_mgmt = NONE
    output=$(nmcli device wifi connect "$SSID" iface wlan0 --timeout 10)
fi

wget -q --tries=5 --timeout=5 --spider http://google.com &> /dev/null # Is connected to Internet?
if [[ $? -eq 0 ]]; then
        echo "conectado a Internet tu ip: "; hostname -I; # Is connected to Internet
        exit 0
else
        echo "Error. $output" # Anything goes wrong
        exit 1
fi

#
# ./ch.sh: vivek-tech.com to nixcraft.com referance converted using this tool
# See the tool at http://www.nixcraft.com/uniqlinuxfeatures/tools/
#

