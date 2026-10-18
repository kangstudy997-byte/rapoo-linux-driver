#!/usr/bin/env bash
# Rapoo Gaming Mouse Native Linux Driver Uninstaller
set -euo pipefail

BIN_DIR="${HOME}/.local/bin"
LIB_DIR="${HOME}/.local/lib"
SYSTEMD_USER_DIR="${HOME}/.config/systemd/user"

echo "Uninstalling Rapoo Gaming Mouse Linux Driver..."

systemctl --user stop rapoo-mouse.service || true
systemctl --user disable rapoo-mouse.service || true
rm -f "${SYSTEMD_USER_DIR}/rapoo-mouse.service"
systemctl --user daemon-reload

rm -f "${BIN_DIR}/rapoo-mouse"
rm -f "${BIN_DIR}/rapoo-mouse-gui"
rm -rf "${LIB_DIR}/rapoo_mouse"

if [ -f "/etc/udev/rules.d/70-rapoo-mouse.rules" ]; then
    echo "Removing udev rule (requires sudo)..."
    sudo rm -f "/etc/udev/rules.d/70-rapoo-mouse.rules"
    sudo udevadm control --reload-rules && sudo udevadm trigger || true
fi

echo "✓ Uninstallation complete. (~/.config/rapoo-mouse profile configs preserved)"
