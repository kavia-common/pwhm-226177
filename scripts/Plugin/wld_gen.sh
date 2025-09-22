#!/bin/sh

. /usr/lib/amx/scripts/amx_init_functions.sh

if [ -e "/lib/functions/system.sh" ]; then
. /lib/functions/system.sh
fi

[ -f /etc/environment ] && source /etc/environment
ulimit -c ${ULIMIT_CONFIGURATION:-0}
name="wld"
datamodel_root="WiFi"
pwhm_term_timeout=15

prevent_netifd_to_configure_wireless()
{
    [ -x /etc/init.d/wpad ] && /etc/init.d/wpad stop
    entries=$(uci -q show wireless 2> /dev/null | grep -e "wifi-iface" -e "wifi-device" | cut -d '=' -f1 | cut -d '.' -f2) && [ -n "$entries" ] || return
    for i in $entries; do
        uci -q del wireless.$i
    done
    uci -q commit wireless
    logger -t $name -p daemon.warn "Wireless is managed by prplMesh Wireless Hardware Manager"
}

get_base_wan_address()
{
    # Define WAN_ADDR env var needed by pwhm to initialize wifi devices
    if [ -e "/etc/environment" ]; then
        set -a
        . /etc/environment 2> /dev/null
        if [ -z "$WAN_ADDR" -a -n "$BASEMACADDRESS" ]; then
            export WAN_ADDR="$BASEMACADDRESS"
        fi
        if [ -z "$WLAN_ADDR" -a -n "$BASEMACADDRESS_PLUS_4" ]; then
            export WLAN_ADDR="$BASEMACADDRESS_PLUS_4"
        fi
        [ -z "$WLAN_ADDR" ] || return
    fi
    if [ -e "/sys/class/net/br-lan" ]; then
        export WLAN_ADDR=$(macaddr_add $(cat /sys/class/net/br-lan/address) +1)
    fi
}

init_wld()
{
    get_base_wan_address
    mkdir -p /var/lib/${name}
    touch /var/lib/${name}/${name}_config.odl

    if [ -f "/usr/lib/amx/${name}/preInit.sh" ]; then
        /usr/lib/amx/${name}/preInit.sh
    fi

    [ -S /var/run/pwhm_usp.sock ] && rm -f /var/run/pwhm_usp.sock

    for FILE in $(ls -1 /usr/lib/amx/${name}/modules/*/modPreInit.sh 2> /dev/null); do
       $FILE
    done

    prevent_netifd_to_configure_wireless

}

case $1 in
    boot)
        init_wld
        process_boot ${name} -D
        ;;
    start)
        init_wld
        process_start ${name} -D
        ;;
    stop)
        process_stop ${name} ${pwhm_term_timeout}
        ;;
    shutdown)
        process_shutdown ${name} ${pwhm_term_timeout}
        ;;
    restart)
        $0 stop
        $0 start
        ;;
    debuginfo)
        process_debug_info ${datamodel_root}
        /usr/lib/debuginfo/debug_wifi.sh
        ;;
    *)
        echo "Usage : $0 [start|boot|stop|shutdown|debuginfo|restart]"
        ;;
esac
