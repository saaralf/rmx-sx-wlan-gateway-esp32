#!/usr/bin/env bash
# uninstall.sh — remove the RMX/SX WLAN Gateway service (keeps backups).
set -euo pipefail

SERVICE_NAME="rmx-sx-gateway"
INSTALL_DIR="/opt/rmx-sx-wlan-gateway"
CONFIG_DIR="/etc/rmx-sx-gateway"
SERVICE_USER="rmxsx"

if [[ $EUID -ne 0 ]]; then echo "Run as root: sudo $0" >&2; exit 1; fi

echo "=== Uninstalling $SERVICE_NAME ==="
systemctl stop "$SERVICE_NAME" 2>/dev/null || true
systemctl disable "$SERVICE_NAME" 2>/dev/null || true
rm -f "/etc/systemd/system/$SERVICE_NAME.service"
systemctl daemon-reload

# remove AP configuration? Ask to avoid surprise. Default: keep.
read -r -p "Also remove hostapd/dnsmasq AP config? [y/N] " ANS
if [[ "$ANS" == "y" || "$ANS" == "Y" ]]; then
  rm -f /etc/dnsmasq.d/rmx-sx-gateway.conf
  rm -f /etc/hostapd/hostapd.conf
  sed -i '/# --- rmx-sx-gateway AP/,/nohook wpa_supplicant/d' /etc/dhcpcd.conf 2>/dev/null || true
  systemctl restart dnsmasq hostapd 2>/dev/null || true
fi

# keep config + logs as backup; remove install dir
rm -rf "$INSTALL_DIR"

if id "$SERVICE_USER" &>/dev/null; then
  userdel "$SERVICE_USER" 2>/dev/null || true
fi

echo "Uninstalled. Config in $CONFIG_DIR and logs in /var/log were kept."
echo "Remove manually if desired: sudo rm -rf $CONFIG_DIR /var/log/rmx-sx-gateway"
