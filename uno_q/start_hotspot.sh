#!/usr/bin/env bash
set -euo pipefail

: "${WIFI_PASSWORD:?Set WIFI_PASSWORD to a WPA2 password (at least 8 characters).}"
WIFI_INTERFACE="${WIFI_INTERFACE:-wlan0}"
WIFI_SSID="${WIFI_SSID:-Mecanum-Teleop}"

if ((${#WIFI_PASSWORD} < 8)); then
  echo "WIFI_PASSWORD must contain at least 8 characters." >&2
  exit 2
fi

if ! command -v nmcli >/dev/null 2>&1; then
  echo "NetworkManager (nmcli) is not available on this UNO Q image." >&2
  exit 1
fi

exec sudo nmcli device wifi hotspot \
  ifname "$WIFI_INTERFACE" \
  ssid "$WIFI_SSID" \
  password "$WIFI_PASSWORD"
