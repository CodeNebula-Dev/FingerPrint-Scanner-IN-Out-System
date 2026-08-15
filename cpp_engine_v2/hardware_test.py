#!/usr/bin/env python3
"""
Hardware Validation Test Runner for Biometric Engine v2.0
Communicates with R307 / R503 Fingerprint Scanner over Serial (Pico 2 W / ESP32 / USB-TTL)
and feeds real captured 512-byte templates directly into libgate_engine_v2.dylib.
"""

import sys
import os
import time
import ctypes
from ctypes import Structure, c_char, c_int, c_bool, c_float, c_uint8, POINTER

# ==============================================================================
# C-ABI STRUCT DEFINITIONS (Matching engine.h)
# ==============================================================================
TEMPLATE_SIZE = 512

class StudentRecord(Structure):
    _fields_ = [
        ("roll_number", c_char * 20),
        ("name", c_char * 100),
        ("program", c_char * 20),
        ("batch", c_char * 10),
        ("year", c_int),
        ("phone_number", c_char * 15),
        ("is_hosteller", c_bool),
        ("encrypted_template", c_uint8 * TEMPLATE_SIZE),
    ]

class MatchResult(Structure):
    _fields_ = [
        ("matched", c_bool),
        ("roll_number", c_char * 20),
        ("name", c_char * 100),
        ("program", c_char * 20),
        ("batch", c_char * 10),
        ("year", c_int),
        ("phone_number", c_char * 15),
        ("is_hosteller", c_bool),
        ("confidence_score", c_float),
        ("match_count", c_int),
    ]

class LogEntry(Structure):
    _fields_ = [
        ("roll_number", c_char * 20),
        ("name", c_char * 100),
        ("year", c_int),
        ("reason", c_char * 50),
        ("gate_count", c_int),
        ("status", c_char * 10),
        ("late_return", c_bool),
        ("timestamps", (c_char * 25) * 20),
        ("timestamp_count", c_int),
    ]

# ==============================================================================
# LOAD C++ ENGINE SHARED LIBRARY
# ==============================================================================
def load_engine():
    lib_paths = [
        os.path.abspath("./build/libgate_engine_v2.dylib"),
        os.path.abspath("../build/libgate_engine_v2.dylib"),
        os.path.abspath("./build/libgate_engine_v2.so"),
        os.path.abspath("./build/gate_engine_v2.dll")
    ]
    
    lib = None
    for p in lib_paths:
        if os.path.exists(p):
            lib = ctypes.CDLL(p)
            print(f"[✓] Loaded Engine v2.0 from: {p}")
            break
            
    if not lib:
        raise FileNotFoundError("Could not find compiled libgate_engine_v2.dylib/.so/.dll! Build engine first.")

    # Configure C-ABI function signatures
    lib.engine_init.argtypes = [ctypes.c_char_p]
    lib.engine_init.restype = c_bool

    lib.engine_shutdown.argtypes = []
    lib.engine_shutdown.restype = None

    lib.student_add.argtypes = [POINTER(StudentRecord)]
    lib.student_add.restype = c_bool

    lib.fingerprint_enroll.argtypes = [ctypes.c_char_p, POINTER(c_uint8), c_int]
    lib.fingerprint_enroll.restype = c_bool

    lib.fingerprint_match.argtypes = [POINTER(c_uint8), c_int]
    lib.fingerprint_match.restype = MatchResult

    lib.log_add_entry.argtypes = [ctypes.c_char_p, POINTER(LogEntry)]
    lib.log_add_entry.restype = c_bool

    return lib

# ==============================================================================
# R307 / R503 SERIAL SCANNER CLIENT
# ==============================================================================
class FingerprintScanner:
    """Handles UART packet communication with R307 / R503 fingerprint sensors."""
    def __init__(self, port, baudrate=57600):
        try:
            import serial
            self.ser = serial.Serial(port, baudrate=baudrate, timeout=2.0)
            print(f"[✓] Connected to Scanner on {port} @ {baudrate} baud")
        except ImportError:
            print("[-] 'pyserial' not installed! Run: pip install pyserial")
            self.ser = None
        except Exception as e:
            print(f"[-] Could not open serial port {port}: {e}")
            self.ser = None

    def send_packet(self, pid, payload):
        """Construct standard R30X frame: EF 01 + Addr + PID + Length + Payload + Sum"""
        header = bytes([0xEF, 0x01])
        addr = bytes([0xFF, 0xFF, 0xFF, 0xFF])
        length = len(payload) + 2
        length_bytes = bytes([(length >> 8) & 0xFF, length & 0xFF])
        
        checksum = pid + (length >> 8) + (length & 0xFF) + sum(payload)
        checksum_bytes = bytes([(checksum >> 8) & 0xFF, checksum & 0xFF])
        
        packet = header + addr + bytes([pid]) + length_bytes + payload + checksum_bytes
        self.ser.write(packet)

    def capture_template_512(self):
        """
        Interacts with the sensor to:
          1. Wait for finger press (GenImg - 0x01)
          2. Convert image to CharBuffer1 (Img2tz - 0x02)
          3. Upload 512-byte template to host (UpChar - 0x08)
        Returns: bytes of length 512, or None on failure.
        """
        if not self.ser:
            print("[-] Serial port not available (Mock mode)")
            return None

        print("\n👉 Please place your finger on the optical/capacitive scanner...")
        # 1. GenImg command
        while True:
            self.send_packet(0x01, bytes([0x01])) # 0x01 = GetImage
            resp = self.ser.read(12)
            if len(resp) >= 10 and resp[9] == 0x00: # Confirmation code 00H = success
                print("  [✓] Finger detected and image captured!")
                break
            time.sleep(0.1)

        # 2. Img2Tz -> Buffer 1
        self.send_packet(0x01, bytes([0x02, 0x01]))
        resp = self.ser.read(12)
        if len(resp) < 10 or resp[9] != 0x00:
            print("[-] Failed to generate character buffer from image.")
            return None

        # 3. UpChar from Buffer 1 to Host
        self.send_packet(0x01, bytes([0x08, 0x01]))
        resp = self.ser.read(12)
        if len(resp) < 10 or resp[9] != 0x00:
            print("[-] Sensor rejected template upload command.")
            return None

        # Read template data packets (512 bytes total, chunked in packets of 128/256 bytes)
        raw_template = bytearray()
        while len(raw_template) < 512:
            chunk_header = self.ser.read(9)
            if len(chunk_header) < 9:
                break
            chunk_len = (chunk_header[7] << 8) | chunk_header[8]
            data_len = chunk_len - 2
            data = self.ser.read(data_len)
            chk = self.ser.read(2) # Checksum
            raw_template.extend(data)

        if len(raw_template) >= 512:
            print("  [✓] Successfully extracted 512-byte raw template from sensor DSP!")
            return bytes(raw_template[:512])
        else:
            print(f"[-] Incomplete template received ({len(raw_template)} / 512 bytes)")
            return None

# ==============================================================================
# MAIN TEST WORKFLOW
# ==============================================================================
def main():
    print("=" * 60)
    print("  HARDWARE VALIDATION: R307/R503 -> C-ABI -> ENGINE v2.0")
    print("=" * 60)

    # 1. Initialize Engine
    engine = load_engine()
    if not engine.engine_init(b"./hardware_test_db"):
        print("[-] Failed to initialize engine database.")
        return
    print("[✓] Engine initialized with BioHashing encryption active.")

    # 2. Connect to Serial Scanner
    port = sys.argv[1] if len(sys.argv) > 1 else "/dev/tty.usbmodem1101" # Default Mac USB CDC port for Pico/ESP32
    scanner = FingerprintScanner(port=port, baudrate=57600)

    while True:
        print("\n" + "=" * 40)
        print(" [1] Enroll New Student with Live Finger")
        print(" [2] Live Scan Gate Verification (Match/Reject)")
        print(" [3] Exit")
        print("=" * 40)
        choice = input("Select an option (1-3): ").strip()

        if choice == "1":
            roll = input("Enter Roll Number (e.g. 2026_CS_001): ").strip()
            name = input("Enter Student Full Name: ").strip()
            hostel = input("Is Hosteller? (y/n): ").strip().lower() == "y"

            # Create Student Record
            rec = StudentRecord()
            rec.roll_number = roll.encode('utf-8')
            rec.name = name.encode('utf-8')
            rec.program = b"B.Tech CS"
            rec.batch = b"2026"
            rec.year = 4
            rec.phone_number = b"+919876543210"
            rec.is_hosteller = hostel

            engine.student_add(ctypes.byref(rec))

            # Capture from Hardware
            if scanner.ser:
                raw_bytes = scanner.capture_template_512()
            else:
                print("  [Mock] Generating synthetic 512-byte fingerprint...")
                raw_bytes = bytes([(i * 3 + 7) % 256 for i in range(512)])

            if raw_bytes:
                c_template = (c_uint8 * TEMPLATE_SIZE).from_buffer_copy(raw_bytes)
                success = engine.fingerprint_enroll(roll.encode('utf-8'), c_template, TEMPLATE_SIZE)
                if success:
                    print(f"🎉 Student {name} enrolled & encrypted into Master Database!")
                else:
                    print("[-] Failed to enroll fingerprint.")

        elif choice == "2":
            print("\n🔍 Ready for Live Scan...")
            if scanner.ser:
                live_bytes = scanner.capture_template_512()
            else:
                print("  [Mock] Generating live scan template...")
                live_bytes = bytes([(i * 3 + 7) % 256 for i in range(512)])

            if live_bytes:
                c_live = (c_uint8 * TEMPLATE_SIZE).from_buffer_copy(live_bytes)
                t0 = time.perf_counter()
                result = engine.fingerprint_match(c_live, TEMPLATE_SIZE)
                dt_ms = (time.perf_counter() - t0) * 1000.0

                print("-" * 40)
                if result.matched:
                    student_name = result.name.decode('utf-8', errors='ignore')
                    roll_num = result.roll_number.decode('utf-8', errors='ignore')
                    print("  🟢 STATUS: MATCH CONFIRMED (ACCESS GRANTED)")
                    print(f"  • Student    : {student_name}")
                    print(f"  • Roll No    : {roll_num}")
                    print(f"  • Confidence : {result.confidence_score * 100.0:.1f}%")
                    print(f"  • Residency  : {'Hosteller' if result.is_hosteller else 'Day Scholar'}")
                    print(f"  • Engine Time: {dt_ms:.2f} ms")
                else:
                    print("  🔴 STATUS: UNKNOWN FINGER (ACCESS DENIED)")
                    print(f"  • Confidence : {result.confidence_score * 100.0:.1f}% (Threshold: 75.0%)")
                    print(f"  • Engine Time: {dt_ms:.2f} ms")
                print("-" * 40)

        elif choice == "3":
            break

    engine.engine_shutdown()
    print("\n[✓] Engine shut down cleanly.")

if __name__ == "__main__":
    main()
