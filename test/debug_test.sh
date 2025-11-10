#!/bin/bash

echo "=== DEBUG TEST ==="
echo "Cleaning up old processes..."
pkill -9 air_c ground radio 2>/dev/null
rm -f /dev/shm/shm_pids_

echo "Compiling with debug info..."
cd ..
gcc -I./air_control_c/include -g -O0 ./air_control_c/src/main.c ./air_control_c/src/functions.c -o test/air_c -lpthread
gcc -I./ground_control/include -g -O0 ./ground_control/src/main.c -o test/ground -lrt
gcc -I./radio/include -g -O0 ./radio/src/main.c -o test/radio -lrt

cd test

echo "Starting air_control..."
timeout 30 ./air_c &
AIR_PID=$!
sleep 1

echo "Starting ground_control..."
timeout 30 ./ground &
GROUND_PID=$!

echo "Waiting for completion (30 second timeout)..."
wait $AIR_PID
AIR_EXIT=$?
wait $GROUND_PID
GROUND_EXIT=$?

echo "Air control exit code: $AIR_EXIT"
echo "Ground control exit code: $GROUND_EXIT"

# Check logs
if [ -f logs/air_control_c.log ]; then
    echo "=== Air Control Log ==="
    tail -20 logs/air_control_c.log
fi

# Cleanup
pkill -9 radio 2>/dev/null
rm -f /dev/shm/shm_pids_
