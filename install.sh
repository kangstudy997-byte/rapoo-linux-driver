#!/usr/bin/env bash
# Rapoo Gaming Mouse Native Linux Driver Installer
# Target: Rapoo VT3PRO MAX / VT1PRO MAX / VT9PRO / VT9PRO 4K (24ae:1244)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="${HOME}/.local/bin"
LIB_DIR="${HOME}/.local/lib"
CONFIG_DIR="${HOME}/.config/rapoo-mouse"
SYSTEMD_USER_DIR="${HOME}/.config/systemd/user"

echo "=========================================================="
echo "  Rapoo Gaming Mouse Linux Driver - Installation Script   "
echo "=========================================================="

# 1. Create target directories
mkdir -p "${BIN_DIR}" "${LIB_DIR}" "${CONFIG_DIR}/profiles" "${SYSTEMD_USER_DIR}"

# 1. Compile C/C++ driver, CLI, and GUI
echo "[1/5] Compiling native C/C++ binaries (gcc)..."
make -C "${SCRIPT_DIR}" all

# 2. Install CLI and GUI executables
echo "[2/5] Installing executables and headers to ${BIN_DIR}..."
make -C "${SCRIPT_DIR}" install

# 4. Install initial profile and macros if not existing
echo "[3/6] Setting up default profile and macro configs..."
if [ ! -f "${CONFIG_DIR}/active_profile.json" ]; then
    cp "${SCRIPT_DIR}/profiles/default.json" "${CONFIG_DIR}/active_profile.json"
fi
if [ ! -f "${CONFIG_DIR}/profiles/default.json" ]; then
    cp "${SCRIPT_DIR}/profiles/default.json" "${CONFIG_DIR}/profiles/default.json"
fi
if [ ! -f "${CONFIG_DIR}/macros.json" ]; then
    cp "${SCRIPT_DIR}/macros/macros.json" "${CONFIG_DIR}/macros.json"
fi

# 5. Install udev rule (requires sudo password once)
echo "[4/6] Installing udev rule for non-root /dev/hidraw access..."
if [ -d "/etc/udev/rules.d" ]; then
    if [ "$EUID" -eq 0 ]; then
        cp "${SCRIPT_DIR}/udev/70-rapoo-mouse.rules" "/etc/udev/rules.d/"
        udevadm control --reload-rules && udevadm trigger
    else
        echo "  - Prompting sudo for udev rule installation..."
        sudo cp "${SCRIPT_DIR}/udev/70-rapoo-mouse.rules" "/etc/udev/rules.d/"
        sudo udevadm control --reload-rules && sudo udevadm trigger || true
    fi
    echo "  ✓ udev rules installed successfully."
fi

# 6. Install and enable systemd user service
echo "[5/6] Enabling systemd user persistence service..."
cp "${SCRIPT_DIR}/systemd/rapoo-mouse.service" "${SYSTEMD_USER_DIR}/"
systemctl --user daemon-reload
systemctl --user enable --now rapoo-mouse.service || true

# 7. Apply settings to physical hardware immediately
echo "[6/6] Applying active profile to physical mouse hardware..."
"${BIN_DIR}/rapoo-mouse" apply || echo "Warning: Device not connected right now. Will apply on plug-in/boot."

echo "=========================================================="
echo "  ✓ Installation Complete!                                "
echo "  - CLI tool : rapoo-mouse status / help                 "
echo "  - GUI tool : rapoo-mouse-gui                           "
echo "=========================================================="
