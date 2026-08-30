#!/bin/sh
set -eu
EXPECTED320=8adf2938b3a7823d2d7005e0c9b8db8f
EXPECTED640=f3f4fcf1a4309205bffa15580daae7da
F320=build320/julia_doom_320.bin
F640=build640/julia_doom_640.bin
[ -f "$F320" ] || { echo "Missing $F320 - run ./build_320.sh"; exit 1; }
[ -f "$F640" ] || { echo "Missing $F640 - run ./build_640.sh"; exit 1; }
H320=$(md5sum "$F320" | awk '{print $1}')
H640=$(md5sum "$F640" | awk '{print $1}')
printf '320: %s  %s\n' "$H320" "$( [ "$H320" = "$EXPECTED320" ] && echo OK || echo FAIL )"
printf '640: %s  %s\n' "$H640" "$( [ "$H640" = "$EXPECTED640" ] && echo OK || echo FAIL )"
[ "$H320" = "$EXPECTED320" ] && [ "$H640" = "$EXPECTED640" ]
