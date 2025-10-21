printCmd(){
    echo "---------------------------------"
    echo -e "${1}"
    echo "---------------------------------"
    ${1}
}

echo "##### pWHM WiFi Debug #####"

echo ""
echo ""
echo "#### iw dev"
iw dev

echo ""
echo ""
echo "#### iw list"
iw list


for FILE in /tmp/*_hapd.conf; do
    echo ""
    echo ""
    echo "#### hostapd config ${FILE}"
    echo ""
    cat $FILE
done


interfaces=$(iw dev | grep Interface | awk '{print $2}' | sort)
for INTF in $interfaces; do
  echo ""
  links=$(iw ${INTF} info | grep link | awk -F: '{print $1}' | cut -d " " -f 2)
  if [ -z "${links}" ]; then
    echo "#### hostapd state ${INTF}"
    printCmd "$SUDO hostapd_cli -p /var/run/hostapd -i ${INTF} driver_flags"
    printCmd "$SUDO hostapd_cli -p /var/run/hostapd -i ${INTF} get_config"
    printCmd "$SUDO hostapd_cli -p /var/run/hostapd -i ${INTF} status"
    printCmd "$SUDO hostapd_cli -p /var/run/hostapd -i ${INTF} all_sta"
    printCmd "$SUDO hostapd_cli -p /var/run/hostapd -i ${INTF} wps_get_status"
  else
    for link in ${links}; do
      echo "#### hostapd state ${INTF} link${link}"
      printCmd "$SUDO hostapd_cli -p /var/run/hostapd -i ${INTF} -l ${link} driver_flags"
      printCmd "$SUDO hostapd_cli -p /var/run/hostapd -i ${INTF} -l ${link} get_config"
      printCmd "$SUDO hostapd_cli -p /var/run/hostapd -i ${INTF} -l ${link} status"
      printCmd "$SUDO hostapd_cli -p /var/run/hostapd -i ${INTF} -l ${link} all_sta"
      printCmd "$SUDO hostapd_cli -p /var/run/hostapd -i ${INTF} -l ${link} wps_get_status"
    done
  fi
done
