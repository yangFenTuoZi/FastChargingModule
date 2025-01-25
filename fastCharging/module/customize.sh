#!/system/bin/sh
abi=$(getprop ro.product.cpu.abi)
fastCharging="${MODPATH:?}/bin/$abi/fastCharging"
[ -f "$fastCharging" ] || {
    ui_print " ! 不支持的ABI: $abi"
    exit 1
}
cp -f "$fastCharging" "${MODPATH:?}/fastCharging"
rm -rf "${MODPATH:?}/bin"
[ -d "/data/adb/fastCharging" ] || {
    mkdir -p "/data/adb/fastCharging"
}
[ -f "/data/adb/fastCharging/config.ini" ] && {
    rm -f "${MODPATH:?}/config.ini"
} || {
    mv "${MODPATH:?}/config.ini" "/data/adb/fastCharging/config.ini"
}
ui_print "**************************************"
ui_print " 注意:"
ui_print "  配置文件需要自己修改后才可以使用!"
ui_print "  config file:"
ui_print "    /data/adb/fastCharging/config.ini"
ui_print "**************************************"
