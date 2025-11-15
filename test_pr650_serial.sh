#!/bin/bash
# Test script for PR650 serial communication
# Usage: ./test_pr650_serial.sh /dev/ttyUSB0

if [ $# -eq 0 ]; then
    echo "Usage: $0 <serial_port>"
    echo "Example: $0 /dev/ttyUSB0"
    echo ""
    echo "Available serial ports:"
    find /dev -name "tty*" -type c 2>/dev/null | grep -E "(USB|ACM|S[0-9])" | sort
    exit 1
fi

PORT=$1

if [ ! -e "$PORT" ]; then
    echo "Error: Port $PORT does not exist"
    exit 1
fi

echo "Testing PR650 on $PORT"
echo "================================"
echo ""
echo "Method 1: Using 'cu' (type '~.' to exit)"
echo "Command: cu -l $PORT -s 9600"
echo ""
echo "Method 2: Using 'screen' (Ctrl+A then K to exit)"
echo "Command: screen $PORT 9600"
echo ""
echo "Method 3: Using 'stty' + 'cat' (Ctrl+C to exit)"
echo "Setting up port..."
stty -F $PORT 9600 cs8 -cstopb -parenb raw
echo "Port configured. Type 'b1' and press Enter, then Ctrl+C to exit:"
cat $PORT &
CAT_PID=$!
sleep 1
echo "b1" > $PORT
sleep 2
kill $CAT_PID 2>/dev/null
echo ""
echo "Test complete. Check output above for PR650 response."

