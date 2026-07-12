#!/usr/bin/env bash
# status.sh — report the gateway service, AP and interface health.
set -euo pipefail

SERVICE_NAME="rmx-sx-gateway"

echo "=== Service status ==="
systemctl is-active "$SERVICE_NAME" 2>/dev/null && \
  systemctl status "$SERVICE_NAME" --no-pager -l | head -20 || \
  echo "$SERVICE_NAME is NOT running"

echo
echo "=== Listening port (8080 expected) ==="
ss -ltnp 2>/dev/null | grep -E ':8080' || echo "port 8080 not listening"

echo
echo "=== WLAN AP (hostapd) ==="
systemctl is-active hostapd 2>/dev/null && echo "hostapd active" || echo "hostapd inactive"
iw dev 2>/dev/null | grep -E 'Interface|ssid' || true

echo
echo "=== DHCP (dnsmasq) ==="
systemctl is-active dnsmasq 2>/dev/null && echo "dnsmasq active" || echo "dnsmasq inactive"

echo
echo "=== Serial devices by-id ==="
ls -l /dev/serial/by-id/ 2>/dev/null || echo "no /dev/serial/by-id devices"

echo
echo "=== Recent log (journalctl) ==="
journalctl -u "$SERVICE_NAME" -n 15 --no-pager 2>/dev/null || \
  tail -n 15 /var/log/rmx-sx-gateway/rmx-sx-gateway.log 2>/dev/null || echo "no logs"
