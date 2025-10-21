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

HOSTAPD_COMMANDS='driver_flags
get_config
status
raw STATUS-DRIVER
all_sta
wps_get_status'

interfaces=$(iw dev | awk '/Interface/ {iface=$2} /type/ && /AP/ {print iface}' | sort)
for INTF in $interfaces; do
  echo ""
  links=$(iw ${INTF} info | grep link | awk -F: '{print $1}' | cut -d " " -f 2)
  [ -z "$links" ] && links="__NO_LINK__"

  for link in ${links}; do
    if [ "${link}" = "__NO_LINK__" ]; then
      echo "#### hostapd state ${INTF}"
      linkArg=""
    else
      echo "#### hostapd state ${INTF} link${link}"
      linkArg="-l ${link}"
    fi

    echo "$HOSTAPD_COMMANDS" | while IFS= read -r cmd; do
      printCmd "$SUDO hostapd_cli -p /var/run/hostapd -i ${INTF} ${linkArg} ${cmd}"
    done
  done
done
