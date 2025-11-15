#!/usr/bin/env python3
"""
Test script for PR650 serial communication
Usage: python3 test_pr650.py <port>
Example: python3 test_pr650.py /dev/ttyUSB0
"""

import sys
import time
import serial
import serial.tools.list_ports


def test_pr650(port_name):
    print(f"Testing PR650 on {port_name}")
    print("=" * 50)

    try:
        # Open serial port with same settings as working Python code
        ser = serial.Serial(port_name, 9600, timeout=10)
        print(f"✓ Opened port {port_name}")

        # Wait 1 second like the working code
        time.sleep(1.0)
        print("✓ Waited 1 second for device to stabilize")

        # Flush input buffer
        ser.reset_input_buffer()
        print("✓ Flushed input buffer")

        # Send b1 command
        print("\nSending 'b1' command...")
        ser.write(b'b1\n')
        ser.flush()
        print("✓ Sent 'b1\\n'")

        # Wait a bit
        time.sleep(0.5)
        print("✓ Waited 500ms")

        # Try to read response
        print("\nReading response (timeout: 10s)...")
        try:
            response = ser.readline()
            if response:
                print(f"✓ Got response: {response!r}")
                print(f"  Hex: {response.hex(' ')}")
                print(f"  Decoded: {response.decode('ascii', errors='replace')!r}")
                if response == b'000\r\n':
                    print("\n✓ SUCCESS! PR650 responded with '000\\r\\n'")
                    return True
                else:
                    print(f"\n⚠ Got response but not '000\\r\\n': {response!r}")
                    return False
            else:
                print("✗ No response received (timeout)")
                return False
        except Exception as e:
            print(f"✗ Error reading: {e}")
            return False

    except serial.SerialException as e:
        print(f"✗ Failed to open serial port: {e}")
        return False
    except Exception as e:
        print(f"✗ Error: {e}")
        return False
    finally:
        try:
            ser.close()
            print("\n✓ Closed serial port")
        except:
            pass


def list_ports():
    print("Available serial ports:")
    print("=" * 50)
    ports = serial.tools.list_ports.comports()
    if ports:
        for port, desc, hwid in ports:
            print(f"  {port:20} - {desc} ({hwid})")
    else:
        print("  No serial ports found")
    print()


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 test_pr650.py <port>")
        print("Example: python3 test_pr650.py /dev/ttyUSB0")
        print()
        list_ports()
        sys.exit(1)

    port = sys.argv[1]
    success = test_pr650(port)
    sys.exit(0 if success else 1)
