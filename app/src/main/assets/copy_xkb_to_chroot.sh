#!/system/bin/sh
# Copy xkb keymap files into chroot if missing
CHROOT_XKB_DIR="$1/usr/share/X11/xkb"
SRC_XKB_DIR="/data/data/com.winland.server/xkb"
if [ ! -d "$CHROOT_XKB_DIR" ]; then
    mkdir -p "$CHROOT_XKB_DIR"
fi
cp -r "$SRC_XKB_DIR"/* "$CHROOT_XKB_DIR" 2>/dev/null || true
