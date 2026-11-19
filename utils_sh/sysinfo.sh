#!/system/bin/sh

# sysinfo - Simple system informations script for Android
# Licensed under the MIT License.

discovercmd() {
    command -v "$1" >/dev/null 2>&1
}

if discovercmd getprop; then
	:
else
	echo "getprop not found!"
	exit 1
fi

KERNEL=$(uname -r)
ANDROID=$(getprop ro.build.version.release)
ANDROID_API_LEVEL=$(getprop ro.build.version.sdk)
ARCHITECTURE=$(uname -m)
USER=$(whoami)
HOSTNAME=$(hostname)
USERHOSTNAME="$USER"@"$HOSTNAME"

echo "$USERHOSTNAME"
echo
echo "--------------"
echo
echo "Host: $HOSTNAME"
echo "Kernel: $KERNEL"
echo "OS: Android $ANDROID $ARCHITECTURE"
echo "Android API level: $ANDROID_API_LEVEL"

