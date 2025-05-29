#!/bin/bash

# Basic test script for wr_ui_runtime

# Navigate to the script's directory to ensure paths are correct
cd "$(dirname "$0")" || exit 1

echo "Attempting to build wr_ui_runtime..."
make clean
make
if [ ! -f ./wr_ui_runtime ]; then
    echo "Build failed. wr_ui_runtime executable not found."
    exit 1
fi
echo "Build successful."

echo ""
echo "Running wr_ui_runtime with test.json..."
echo "The UI window should appear shortly."
echo ""
echo "MANUAL TEST REQUIRED FOR BUTTON CLICK:"
echo "1. Click the 'Click Me for Action!' button in the UI window."
echo "2. Check the terminal output from where you ran this script."
echo "3. You should see the following JSON action printed to stdout:"
echo "   {\"type\":\"test_click\",\"payload\":\"Button 'Click Me for Action!' was successfully clicked.\",\"timestamp\":\"2023-10-27T10:00:00Z\"}"
echo ""
echo "The application will run with test.json. Close the UI window to end the test."
echo "(If it doesn't close automatically or you run it in background, you might need to kill it manually)"
echo ""

cat ./test.json | ./wr_ui_runtime

echo ""
echo "Test finished. Check output above for button action if clicked."
