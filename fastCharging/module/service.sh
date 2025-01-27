#!/system/bin/sh
MODDIR=${0%/*}
until [[ "$(getprop sys.boot_completed)" == "1" ]]; do
    sleep 1
done
[ -x $MODDIR/fastCharging ] || chmod +x $MODDIR/fastCharging
killall fastCharging
unshare --propagation slave -m sh -c "$MODDIR/fastCharging $@&"
