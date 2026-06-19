#!/bin/bash

# Define the build directory for the daemon
BUILD_DIR="./ulysses-daemon/build-Ulysses-Desktop_Qt_5_15_2_GCC_64bit-Debug"
UI_DIR="./ulysses-ui"

# Ensure the binaries exist
if [ ! -f "$BUILD_DIR/ulyssesd" ]; then
    echo "Error: ulyssesd not found in $BUILD_DIR"
    echo "Please build the daemon project first."
    exit 1
fi

echo "--- Setting capabilities for ulyssesd (may require sudo password) ---"
sudo setcap 'cap_net_admin,cap_sys_ptrace,cap_kill+eip' "$BUILD_DIR/ulyssesd"

echo "--- Stopping any existing ulyssesd processes ---"
killall ulyssesd 2>/dev/null
# Also aggressively kill the unkillable daemon by finding who holds the DB open
fuser -k -9 ~/.config/ulysses/ulysses.db 2>/dev/null
sleep 1

echo "--- Starting ulyssesd (Daemon) in the background (Dev Mode) ---"
"$BUILD_DIR/ulyssesd" --dev &
DAEMON_PID=$!

# Give the daemon a moment to register on the D-Bus
sleep 1 

echo "--- Starting Tauri UI ---"
cd "$UI_DIR"
npm run tauri dev

# When the GUI is closed, we kill the daemon
echo "--- Tauri UI closed. Stopping daemon... ---"
kill $DAEMON_PID 2>/dev/null
