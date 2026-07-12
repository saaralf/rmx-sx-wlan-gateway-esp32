#!/usr/bin/env bash
# install.sh — idempotent installer for the RMX/SX WLAN Gateway on a Raspberry Pi.
#
# What it does:
#   * installs system packages (python3-venv, hostapd, dnsmasq, ...)
#   * creates a service user (rmxsx) in group dialout for serial access
#   * installs the Python package into /opt/rmx-sx-wlan-gateway/.venv
#   * installs config to /etc/rmx-sx-gateway/gateway.yaml (backs up existing)
#   * installs the systemd unit and (optionally) configures the AP
#
# Safe to run repeatedly. Existing config is preserved unless --force-config.
set -euo pipefail

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INSTALL_DIR="/opt/rmx-sx-wlan-gateway"
CONFIG_DIR="/etc/rmx-sx-gateway"
CONFIG_FILE="$CONFIG_DIR/gateway.yaml"
LOG_DIR="/var/log/rmx-sx-gateway"
RUN_DIR="/run/rmx-sx-gateway"
SERVICE_USER="rmxsx"
SERVICE_NAME="rmx-sx-gateway"

FORCE_CONFIG=0
SETUP_AP=0
SKIP_PIP=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --force-config) FORCE_CONFIG=1; shift;;
    --setup-ap)     SETUP_AP=1; shift;;
    --skip-pip)     SKIP_PIP=1; shift;;
    *) echo "unknown arg: $1" >&2; exit 2;;
  esac
done

if [[ $EUID -ne 0 ]]; then
  echo "Run as root: sudo $0" >&2; exit 1
fi

echo "=== Installing RMX/SX WLAN Gateway from $SRC_DIR ==="

export DEBIAN_FRONTEND=noninteractive
apt-get update -y
apt-get install -y python3 python3-venv python3-pip hostapd dnsmasq \
  python3-systemd 2>/dev/null || apt-get install -y python3 python3-venv

# service user
if ! id "$SERVICE_USER" &>/dev/null; then
  useradd --system --no-create-home --shell /usr/sbin/nologin "$SERVICE_USER"
fi
usermod -aG dialout "$SERVICE_USER" 2>/dev/null || true

# directories
mkdir -p "$INSTALL_DIR" "$CONFIG_DIR" "$LOG_DIR" "$RUN_DIR"
chown -R "$SERVICE_USER:dialout" "$LOG_DIR" "$RUN_DIR"

# copy sources
rm -rf "$INSTALL_DIR/src" && cp -r "$SRC_DIR/src" "$INSTALL_DIR/src"
cp -f "$SRC_DIR/pyproject.toml" "$SRC_DIR/requirements.txt" "$INSTALL_DIR/" 2>/dev/null || true
cp -rf "$SRC_DIR/config" "$INSTALL_DIR/config" 2>/dev/null || true

# python venv
if [[ $SKIP_PIP -eq 0 ]]; then
  python3 -m venv "$INSTALL_DIR/.venv"
  # shellcheck disable=SC1091
  . "$INSTALL_DIR/.venv/bin/activate"
  pip install --upgrade pip -q
  pip install -e "$INSTALL_DIR" -q || pip install -r "$INSTALL_DIR/requirements.txt" -q
  deactivate 2>/dev/null || true
fi

# config
if [[ ! -f "$CONFIG_FILE" || $FORCE_CONFIG -eq 1 ]]; then
  cp -f "$SRC_DIR/config/gateway.example.yaml" "$CONFIG_FILE"
  echo "Installed config to $CONFIG_FILE"
fi
chown "$SERVICE_USER:dialout" "$CONFIG_FILE"

# systemd unit
cp -f "$SRC_DIR/systemd/$SERVICE_NAME.service" /etc/systemd/system/$SERVICE_NAME.service
sed -i "s|/opt/rmx-sx-wlan-gateway/.venv|$INSTALL_DIR/.venv|g" /etc/systemd/system/$SERVICE_NAME.service
sed -i "s|/etc/rmx-sx-gateway/gateway.yaml|$CONFIG_FILE|g" /etc/systemd/system/$SERVICE_NAME.service
sed -i "s|^WorkingDirectory=.*|WorkingDirectory=$INSTALL_DIR|g" /etc/systemd/system/$SERVICE_NAME.service
systemctl daemon-reload
systemctl enable "$SERVICE_NAME"

# optional AP
if [[ $SETUP_AP -eq 1 ]]; then
  echo "=== Setting up WLAN AP ==="
  bash "$SRC_DIR/scripts/configure_ap.sh" || echo "AP setup had issues; check status.sh"
fi

echo "=== Install complete. ==="
echo "Start service:  sudo systemctl start $SERVICE_NAME"
echo "Check status:   sudo ./status.sh"
echo "Configure AP:   sudo ./configure_ap.sh  (or install with --setup-ap)"
