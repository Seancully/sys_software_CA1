#!/bin/bash
# Test script for company daemon

echo "Creating necessary directories..."
mkdir -p data/upload data/reporting data/backup logs

echo "Copying test XML files to upload directory..."
cp test_files/*.xml data/upload/

echo "Building the daemon..."
make

echo "Starting the daemon in test mode..."
./bin/company_daemon &
DAEMON_PID=$!
echo "Daemon started with PID: $DAEMON_PID"

# Let the daemon initialize
sleep 2

echo "Checking daemon status..."
if kill -0 $DAEMON_PID 2>/dev/null; then
    echo "Daemon is running correctly"
else
    echo "ERROR: Daemon failed to start or crashed"
    exit 1
fi

echo "Triggering manual backup and transfer..."
kill -USR1 $DAEMON_PID

echo "Waiting for operations to complete..."
sleep 5

echo "Checking results..."
echo -e "\nContents of reporting directory:"
ls -la data/reporting/

echo -e "\nContents of backup directory:"
ls -la data/backup/

echo -e "\nLog entries:"
cat logs/error.log

echo -e "\nStopping daemon..."
kill $DAEMON_PID

echo "Test completed successfully!"