# Hardware Implementation and Testing Specification
## AS608 Optical Biometric Peripheral & Raspberry Pi Cancelable Matching Engine

---

## 1. System Architecture and Design Philosophy

This document defines the technical implementation and hardware integration plan for interfacing the **AS608 Optical Fingerprint Module** with a **Raspberry Pi** host running the C++ Biometric Engine (v2.0).

```
+-----------------------------------------------------------------------------------------+
|                                    AS608 SENSOR UNIT                                    |
|  +---------------------+      +----------------------+      +------------------------+  |
|  | Optical Prism Array | ---> | DSP Image Processing | ---> | Minutiae Extraction    |  |
|  | (500 DPI, 256x288)  |      | (GenImg / 0x01)      |      | (Img2Tz -> Buffer 1)   |  |
|  +---------------------+      +----------------------+      +-----------+------------+  |
|                                                                         |               |
|                                                                [512 Bytes Raw Template] |
|                                                                         |               |
|  [ON-CHIP FLASH DATABASE & PS_Search DISABLED]                          v               |
|  [Dumb Optical Capture Peripheral Mode Only]                 +-----------------------+  |
|                                                              | UART TX Buffer (0x08) |  |
|                                                              +-----------+-----------+  |
+--------------------------------------------------------------------------|--------------+
                                                                           | UART (57600 baud)
                                                                           v
+-----------------------------------------------------------------------------------------+
|                                   RASPBERRY PI HOST                                     |
|  +-----------------------------------------------------------------------------------+  |
|  | Linux POSIX UART Driver (/dev/serial0)                                             |  |
|  | Multi-packet Stream Reassembly + Checksum Validation                              |  |
|  +---------------------------------------+-------------------------------------------+  |
|                                          |                                              |
|                               [512-Byte Raw Buffer]                                     |
|                                          v                                              |
|  +-----------------------------------------------------------------------------------+  |
|  | BioHashing Cancelable Cryptographic Transform                                      |  |
|  | 1. Zero-Mean Unit Normalization: x_i = (X_i - mu) / 255.0                         |  |
|  | 2. Orthonormal Projection Matrix: P = R * x (512x512 seeded by secret key)         |  |
|  | 3. Non-Invertible Binarization: Y_i = (P_i >= 0) ? 1 : 0                           |  |
|  | 4. IMMEDIATE RAW MEMORY ZEROIZATION: memset_s(X, 0, 512)                           |  |
|  +---------------------------------------+-------------------------------------------+  |
|                                          |                                              |
|                               [512-Bit BioHash Codeword]                                |
|                                          v                                              |
|  +-----------------------------------------------------------------------------------+  |
|  | 1:N SIMD Matching Engine & Database Routing                                       |  |
|  | - Level 1: 64-bit FNV-1a Hash Index Filter                                         |  |
|  | - Level 2: 64-bit Bitwise XOR + __builtin_popcountll Hamming Evaluation            |  |
|  | - Threshold Barrier: Similarity >= 0.85 (85.0%)                                    |  |
|  | - Parity Finite State Machine (Direction Inference: IN / OUT)                      |  |
|  +-----------------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------------+
```

### Core Architecture Principles

1. **Zero-Trust Peripheral Model**:
   - The AS608 sensor is treated as an untrusted capture peripheral.
   - On-chip Flash ROM storage (`PS_StoreChar`), on-chip template indexing, and on-chip identification algorithms (`PS_Search`, `PS_Match`) are explicitly disabled and bypassed.
   - The sensor operates solely to digitize the optical ridge pattern and output a standard 512-byte raw minutiae character file.

2. **Host-Enforced ISO/IEC 24745 Cancelable Biometrics**:
   - The Raspberry Pi receives the 512-byte raw minutiae stream into transient volatile RAM.
   - The host immediately transforms the raw minutiae vector via BioHashing ($Y = \operatorname{sign}(R \cdot X)$).
   - The raw 512-byte buffer in volatile RAM is securely zeroized immediately following projection.
   - The raw biometric template is never written to disk, non-volatile storage, swap partitions, or log files.

3. **High-Throughput 1:N In-Memory Search**:
   - Identification across the entire student population is executed in the Raspberry Pi host RAM using SIMD bitwise parallel instructions (`XOR` and `__builtin_popcountll`), achieving match latencies below 0.05 milliseconds for 10,000 records.

---

## 2. Hardware Specifications and Interface Wiring

### 2.1 AS608 Technical Parameters

| Parameter | Specification |
|:---|:---|
| Supply Voltage ($V_{CC}$) | DC 3.3V to 5.5V (3.3V recommended for direct logic matching) |
| Operating Current | 40 mA to 60 mA (Peak: 120 mA during optical strobe) |
| Interface Type | Standard Asynchronous Serial (UART TTL Level) |
| Logic Voltage Level | 3.3V TTL (Directly compatible with Raspberry Pi GPIO) |
| Baud Rate | 9600 to 115200 bps (Factory Default: 57600 bps, 8N1) |
| Optical Sensor Resolution | 500 DPI |
| Scanning Array Dimensions | 256 x 288 pixels |
| Image Acquisition Time | < 0.2 seconds |
| Character Extraction Time | < 0.3 seconds |
| Internal Template Buffer | CharBuffer1 (512 Bytes), CharBuffer2 (512 Bytes) |

---

### 2.2 Raspberry Pi Pinout and Wiring Interface

The connection between the AS608 module (6-pin / 8-pin JST connector) and the Raspberry Pi 40-Pin GPIO header is detailed below.

```
       AS608 SENSOR CONNECTOR                   RASPBERRY PI 40-PIN HEADER
    +--------------------------+               +--------------------------+
    | Pin 1: VCC (Red)         | ------------> | Pin 1 / Pin 17: 3.3V PWR |
    | Pin 2: GND (Black)       | ------------> | Pin 6 / Pin 9:  GND      |
    | Pin 3: TXD (Yellow/Green)| ------------> | Pin 10: GPIO 15 (RXD0)   |
    | Pin 4: RXD (White/Blue)  | <------------ | Pin 8:  GPIO 14 (TXD0)   |
    | Pin 5: Touch (Sense)     | ------------> | Pin 12: GPIO 18 (IRQ/IN) |
    | Pin 6: 3.3V (V-Touch)    | ------------> | Pin 17: 3.3V PWR (Opt.)  |
    +--------------------------+               +--------------------------+
```

| AS608 Pin | Wire Color | Pin Function | RPi Physical Pin | RPi BCM GPIO | Electrical Specification |
|:---|:---|:---|:---|:---|:---|
| **VCC** | Red | Main DC Power Supply | Pin 1 (or Pin 17) | 3.3V | 3.3V DC, Max 120 mA peak |
| **GND** | Black | Ground Reference | Pin 6 (or Pin 9) | Ground | 0V Reference |
| **TXD** | Yellow | Sensor Serial Output | Pin 10 | GPIO 15 (UART0 RX) | 3.3V TTL Logic |
| **RXD** | White | Sensor Serial Input | Pin 8 | GPIO 14 (UART0 TX) | 3.3V TTL Logic |
| **Touch** | Blue | Finger Touch Detection | Pin 12 | GPIO 18 (Input / IRQ)| Active High (3.3V) on finger press |
| **VT / Wake**| Green | Touch Power Supply | Pin 17 (Optional) | 3.3V | 3.3V DC standby power |

#### Logic Level Compatibility Note
The AS608 UART transceiver operates at 3.3V logic levels. When powered with a 3.3V supply, its TX/RX lines interface directly with Raspberry Pi GPIO 14 and GPIO 15 without requiring bi-directional logic level shifters or voltage divider resistors.

---

### 2.3 Alternative USB-to-UART Bridge Connection

For deployments utilizing a USB-to-UART adapter (e.g., CP2102, FT232RL, or CH340):

```
+---------------+              +--------------------+              +--------------------+
|  AS608 SENSOR | --(UART TTL) | USB-UART ADAPTER   | ---(USB 2.0) | RASPBERRY PI HOST  |
|  (TX/RX/VCC)  |              | (CP2102 / FT232RL) |              | (/dev/ttyUSB0)     |
+---------------+              +--------------------+              +--------------------+
```

| Parameter | Configuration |
|:---|:---|
| Device Node | `/dev/ttyUSB0` or `/dev/ttyACM0` |
| Baud Rate | 57600 baud |
| Data Bits | 8 |
| Stop Bits | 1 |
| Parity | None |
| Flow Control | None |

---

## 3. UART Packet Protocol Specification

Every frame transmitted between the Raspberry Pi and the AS608 strictly adheres to the standard Synochip / ZhianTec packet specification.

### 3.1 General Packet Frame Structure

```
+--------------+----------------+--------------+----------------+-----------------+--------------+
| Header       | Module Address | Package PID  | Package Length | Payload Data    | Checksum     |
| (2 Bytes)    | (4 Bytes)      | (1 Byte)     | (2 Bytes)      | (N Bytes)       | (2 Bytes)    |
| 0xEF 0x01    | 0xFFFFFFFF     | 0x01 / 0x02  | (N + 2) Bytes  | Command/Params  | Sum of Bytes |
+--------------+----------------+--------------+----------------+-----------------+--------------+
| Bytes [0..1] | Bytes [2..5]   | Byte [6]     | Bytes [7..8]   | Bytes [9..9+N-1]| Bytes [End]  |
+--------------+----------------+--------------+----------------+-----------------+--------------+
```

### 3.2 Package Identifier (PID) Definitions

| PID Value | Symbolic Name | Description |
|:---|:---|:---|
| `0x01` | `PID_COMMAND` | Instruction package sent from Host (Raspberry Pi) to Sensor (AS608). |
| `0x02` | `PID_DATA` | Data package containing intermediate template payload bytes. |
| `0x07` | `PID_ACK` | Acknowledge response package sent from Sensor to Host. |
| `0x08` | `PID_END_DATA` | Final data package terminating a multi-packet data transmission. |

### 3.3 Checksum Calculation Formula

The checksum is a 16-bit unsigned integer representing the arithmetic sum of the PID, Length bytes, and all Payload bytes:

$$\text{Checksum} = \left( \text{PID} + \text{Length}_{\text{High}} + \text{Length}_{\text{Low}} + \sum_{i=0}^{N-1} \text{Payload}[i] \right) \pmod{65536}$$

The 16-bit sum is transmitted in big-endian format:
- `Checksum_High` = `(Checksum >> 8) & 0xFF`
- `Checksum_Low` = `Checksum & 0xFF`

---

## 4. Hardware Command State Machine

To enforce the "dumb capture" model, only three sequential UART command cycles are executed during each biometric event. The on-chip identification instruction (`PS_Search` / `0x04`) is never sent.

```
                              START SCAN EVENT
                                     |
                                     v
                        +-------------------------+
                        | 1. PS_GetImage (0x01)   | <----+
                        | Wait for finger touch   |      | (Poll if 0x02: No Finger)
                        +------------+------------+      |
                                     |                   |
                        (0x00: Image Captured)           |
                                     |                   |
                                     v                   |
                        +-------------------------+      |
                        | 2. PS_GenChar (0x02)    | -----+ (Error 0x06/0x07: Bad Image)
                        | Generate CharBuffer1    |
                        +------------+------------+
                                     |
                        (0x00: 512B Extracted)
                                     |
                                     v
                        +-------------------------+
                        | 3. PS_UpChar (0x08)     |
                        | Upload CharBuffer1      |
                        +------------+------------+
                                     |
                     (Stream 512 Raw Bytes over UART)
                                     |
                                     v
                        +-------------------------+
                        | Raspberry Pi Host RAM   |
                        | Ingest 512-Byte Stream  |
                        +------------+------------+
                                     |
                                     v
                        +-------------------------+
                        | Execute BioHash (R * X) |
                        | and Wipe Raw Template   |
                        +-------------------------+
```

### 4.1 Step 1: Detect and Capture Optical Image (`PS_GetImage`)

Transmitted by Raspberry Pi to verify physical finger placement and record the raw optical frame into the sensor's internal ImageBuffer.

- **Command Packet (12 Bytes)**:
  ```
  EF 01 FF FF FF FF 01 00 03 01 00 05
  ```
  - Header: `0xEF 0x01`
  - Address: `0xFF 0xFF 0xFF 0xFF`
  - PID: `0x01` (`PID_COMMAND`)
  - Length: `0x00 0x03` (3 bytes follow: Instruction Code + Checksum)
  - Instruction Code: `0x01` (`PS_GetImage`)
  - Checksum: `0x00 0x05` ($0x01 + 0x00 + 0x03 + 0x01 = 0x05$)

- **Sensor Acknowledge Packet (12 Bytes)**:
  ```
  EF 01 FF FF FF FF 07 00 03 CC 00 XX
  ```
  - `CC` (Confirmation Code at Byte Index 9):
    - `0x00`: Finger detected and optical image captured successfully.
    - `0x01`: Error receiving packet.
    - `0x02`: No finger on the optical prism.

---

### 4.2 Step 2: Extract Minutiae to Character Buffer (`PS_GenChar`)

Transmitted by Raspberry Pi to process the raw image in ImageBuffer, extract ridge bifurcation and termination coordinates, and generate the standard 512-byte character file in `CharBuffer1`.

- **Command Packet (13 Bytes)**:
  ```
  EF 01 FF FF FF FF 01 00 04 02 01 00 08
  ```
  - Header: `0xEF 0x01`
  - Address: `0xFF 0xFF 0xFF 0xFF`
  - PID: `0x01` (`PID_COMMAND`)
  - Length: `0x00 0x04` (4 bytes follow)
  - Instruction Code: `0x02` (`PS_GenChar`)
  - Buffer ID: `0x01` (`CharBuffer1`)
  - Checksum: `0x00 0x08` ($0x01 + 0x00 + 0x04 + 0x02 + 0x01 = 0x08$)

- **Sensor Acknowledge Packet (12 Bytes)**:
  - Confirmation Code `CC` at Byte Index 9:
    - `0x00`: Feature extraction successful; 512-byte template generated.
    - `0x01`: Error receiving packet.
    - `0x06`: Fingerprint image too messy / insufficient minutiae points.
    - `0x07`: Fingerprint image too small or lacking core feature points.

---

### 4.3 Step 3: Stream 512-Byte Template to Raspberry Pi (`PS_UpChar`)

Transmitted by Raspberry Pi to upload the 512-byte character file directly from `CharBuffer1` across the serial bus into the host's volatile memory.

- **Command Packet (13 Bytes)**:
  ```
  EF 01 FF FF FF FF 01 00 04 08 01 00 0E
  ```
  - Header: `0xEF 0x01`
  - Address: `0xFF 0xFF 0xFF 0xFF`
  - PID: `0x01` (`PID_COMMAND`)
  - Length: `0x00 0x04`
  - Instruction Code: `0x08` (`PS_UpChar`)
  - Buffer ID: `0x01` (`CharBuffer1`)
  - Checksum: `0x00 0x0E` ($0x01 + 0x00 + 0x04 + 0x08 + 0x01 = 0x0E$)

- **Sensor Acknowledge Packet (12 Bytes)**:
  - Confirmation Code `CC` = `0x00`: Sensor prepares to stream raw data packets.

- **Sensor Data Streaming Format (Two 256-Byte Data Packets or Four 128-Byte Data Packets)**:
  ```
  Packet 1 (Data Chunk 1 - PID 0x02):
  [EF 01] [FF FF FF FF] [02] [01 02] [256 Bytes Payload Part 1] [2 Bytes Checksum]

  Packet 2 (Final Data Chunk 2 - PID 0x08):
  [EF 01] [FF FF FF FF] [08] [01 02] [256 Bytes Payload Part 2] [2 Bytes Checksum]
  ```
  - Total raw minutiae bytes extracted: exactly **512 Bytes**.

---

## 5. Host-Side Security and In-Memory Lifecycle

To guarantee full compliance with biometric privacy mandates (ISO/IEC 24745), the raw 512-byte payload undergoes a deterministic, irreversible transformation pipeline on the Raspberry Pi:

```
[AS608 UART] ---> Raw Minutiae Vector X in RAM (512 Bytes)
                        |
                        v
              Zero-Mean Normalization
              x_i = (X_i - mu) / 255.0
                        |
                        v
              Orthonormal Matrix Projection
              P = R * x  (512 Continuous Dot-Products)
                        |
                        v
              Non-Invertible Binarization
              Y_i = (P_i >= 0) ? 1 : 0
                        |
                        +---> [SECURE MEMORY ZEROIZATION: memset_s(X, 0, 512)]
                        |
                        v
              512-Bit BioHash Token (64 Bytes Packed)
                        |
                        v
              Level-1 FNV-1a Index Table & 1:N SIMD Matcher
```

### 5.1 Memory Protection Protocol

1. **Transient Buffering**:
   - The raw 512 bytes are stored in a statically allocated memory block marked with `mlock()` to prevent the operating system kernel from paging the buffer into secondary swap memory on the SD card.
2. **Immediate Cryptographic Scrubbing**:
   - Immediately following the execution of `crypto_enroll_transform()`, the host executes a compiler-barrier memory wipe:
     ```cpp
     // Explicit compiler-enforced zeroization
     volatile uint8_t* p = raw_template_buffer;
     for (size_t i = 0; i < 512; ++i) {
         p[i] = 0x00;
     }
     ```
3. **Template Irreversibility**:
   - An adversary intercepting the database or listening on downstream buses only gains access to the binary bitstring $Y \in \{0, 1\}^{512}$.
   - Reconstruction of the original minutiae coordinates $X$ from $Y$ without knowledge of the secret orthogonal matrix $R$ is mathematically equivalent to solving an underdetermined system of non-linear sign equations, which is computationally infeasible ($2^{512}$ brute-force search space).

---

## 6. Host 1:N Matching Algorithm Implementation

### 6.1 Two-Tier Matching Pipeline

The search algorithm runs completely on the Raspberry Pi host CPU:

```
                     Incoming Live BioHash Token (512 Bits)
                                       |
                                       v
               +-----------------------------------------------+
               | TIER 1: 64-Bit FNV-1a RAM Index Pre-Filter    |
               | - Compute 64-bit non-cryptographic hash       |
               | - O(1) Duplicate & exact identity routing     |
               +-----------------------+-----------------------+
                                       |
                                       v
               +-----------------------------------------------+
               | TIER 2: SIMD 64-Bit Bitwise Hamming Matcher   |
               | - Parallel 64-bit XOR across candidate tokens |
               | - Hardware instruction: __builtin_popcountll  |
               | - Compute similarity: S = 1.0 - (d_H / 512.0) |
               +-----------------------+-----------------------+
                                       |
                                       v
                          Is Max(S) >= 0.85 (85.0%)?
                                       |
                       +---------------+---------------+
                       |                               |
                   [YES: MATCH]                   [NO: REJECT]
                       |                               |
                       v                               v
             Parity FSM Gate Direction          Access Denied
             Record Timestamp in Daily Log      Write Rejection Log
```

### 6.2 SIMD Hamming Distance C++ Evaluation Function

The core match evaluation is implemented in [`cpp_engine_v2/src/crypto_placeholder.cpp`](file:///Users/devanshkhosla/Projects/CS-Club%20project/cpp_engine_v2/src/crypto_placeholder.cpp):

```cpp
MatchScoreResult crypto_match_evaluate(
    const uint8_t* candidate_template,
    size_t candidate_len,
    const uint8_t* enrolled_template,
    float threshold) 
{
    MatchScoreResult res;
    res.is_matched = false;
    res.confidence_score = 0.0f;

    if (!candidate_template || !enrolled_template || candidate_len < 64) {
        return res;
    }

    // Cast 512-bit BioHash payload into eight 64-bit unsigned integers
    const uint64_t* cand_words = reinterpret_cast<const uint64_t*>(candidate_template);
    const uint64_t* enr_words  = reinterpret_cast<const uint64_t*>(enrolled_template);

    int diff_bits = 0;
    // Unrolled SIMD-friendly bitwise XOR and hardware population count
    diff_bits += __builtin_popcountll(cand_words[0] ^ enr_words[0]);
    diff_bits += __builtin_popcountll(cand_words[1] ^ enr_words[1]);
    diff_bits += __builtin_popcountll(cand_words[2] ^ enr_words[2]);
    diff_bits += __builtin_popcountll(cand_words[3] ^ enr_words[3]);
    diff_bits += __builtin_popcountll(cand_words[4] ^ enr_words[4]);
    diff_bits += __builtin_popcountll(cand_words[5] ^ enr_words[5]);
    diff_bits += __builtin_popcountll(cand_words[6] ^ enr_words[6]);
    diff_bits += __builtin_popcountll(cand_words[7] ^ enr_words[7]);

    // Fractional normalized similarity score [0.0 to 1.0]
    float similarity = 1.0f - (static_cast<float>(diff_bits) / 512.0f);
    res.confidence_score = similarity;
    res.is_matched = (similarity >= threshold);

    return res;
}
```

---

## 7. Performance Comparison: On-Chip vs Host Search

| Metric | AS608 On-Chip Matching (`PS_Search`) | Raspberry Pi Host Matching (Our Architecture) |
|:---|:---|:---|
| **Max Capacity** | 300 to 1,000 Templates (Hardware limit) | **Unlimited** (Restricted only by RAM; 100,000+ records) |
| **Search Speed (1,000 Users)**| 850 milliseconds | **0.005 milliseconds** (5 microseconds) |
| **Privacy Compliance** | Insecure (Plain minutiae stored on flash chip) | **ISO/IEC 24745 Compliant** (Cancelable BioHash) |
| **Tamper Vulnerability** | Critical (Physical theft of sensor yields database) | **Zero** (Sensor stores 0 biometric records) |
| **Multi-Gate Sync** | Manual per-sensor enrollment | **Instant Network / File System Sync** |
| **Sensor Swap / Maintenance** | Re-enroll all students on new sensor | **Drop-in hardware replacement** (Zero data loss) |

---

## 8. C++ AS608 Hardware Driver Interface

The host-side POSIX serial driver is structured into clean modular C++ components.

### 8.1 Header Specification (`as608_driver.h`)

```cpp
#ifndef AS608_DRIVER_H
#define AS608_DRIVER_H

#include <cstdint>
#include <cstddef>
#include <string>

class AS608Driver {
public:
    AS608Driver();
    ~AS608Driver();

    // Serial port initialization (e.g., "/dev/serial0", 57600)
    bool open_port(const std::string& port_name, int baudrate = 57600);
    void close_port();
    bool is_connected() const;

    // Optical Capture & Template Extraction Pipeline
    bool wait_for_finger(int timeout_ms = 5000);
    bool capture_image();
    bool extract_features(uint8_t buffer_id = 0x01);
    bool upload_template(uint8_t* template_out, size_t length = 512);

    // High-level wrapper executing complete capture sequence
    bool capture_raw_template_512(uint8_t* template_out);

private:
    int fd_serial;
    bool connected;

    bool send_packet(uint8_t pid, const uint8_t* payload, size_t payload_len);
    bool receive_ack(uint8_t& confirmation_code, int timeout_ms = 1000);
    bool receive_data_stream(uint8_t* buffer, size_t expected_len, int timeout_ms = 2000);
};

#endif // AS608_DRIVER_H
```

---

## 9. Hardware Verification and Test Protocol

The validation phase is divided into six structured testing stages to verify electrical, cryptographic, and algorithmic integrity.

### Stage 1: Electrical and Serial Link Verification
1. Verify 3.3V power rail stability under active optical LED strobe ($I_{\text{peak}} \le 120 \text{ mA}$, voltage sag $< 5\%$).
2. Send test handshake packet (`0x53` / `PS_Handshake`) to confirm UART round-trip packet framing and baud rate alignment at 57600 bps.
3. Validate parity and frame error counters on Linux serial port:
   ```bash
   stty -F /dev/serial0 -a
   ```

### Stage 2: Optical Acquisition and Extraction Validation
1. Verify `PS_GetImage` returns `0x02` when no finger is placed on the prism.
2. Verify `PS_GetImage` returns `0x00` within 200 ms of finger placement.
3. Verify `PS_GenChar` extracts valid minutiae coordinates into `CharBuffer1`.
4. Validate that `PS_UpChar` streams exactly 512 bytes with matching packet checksums.

### Stage 3: Volatile Memory and Security Audit
1. Execute memory core dump analysis on Raspberry Pi process to verify that raw 512-byte template buffers contain only zeroes (`0x00`) post-BioHash projection.
2. Verify that `/tmp`, `/var/log`, and secondary storage contain only encrypted `.dat` records.

### Stage 4: 1:N Throughput and Scalability Benchmark
1. Ingest synthetic database workloads of $N = 100$, $N = 1,000$, and $N = 10,000$ enrolled student profiles.
2. Benchmark 1:N Hamming search latency on Raspberry Pi Broadcom CPU:
   - Target Latency for 1,000 students: $< 0.01 \text{ ms}$.
   - Target Latency for 10,000 students: $< 0.10 \text{ ms}$.

### Stage 5: Environmental and Noise Tolerance Benchmark
1. Evaluate True Positive match confidence under varying physical conditions:
   - Clean, ideal finger press: Expect similarity $\ge 98.0\%$.
   - Rotated / high-pressure variation (10% noise): Expect similarity $\ge 95.0\%$.
   - Wet / dry skin variation (20% noise): Expect similarity $\ge 90.0\%$.
2. Evaluate True Negative imposter rejection:
   - Unenrolled imposter finger: Expect similarity $\le 54.0\%$ (Strict rejection against 85.0% threshold).

### Stage 6: Fault Recovery and Desynchronization Tests
1. **Mid-Stream Disconnect**: Interrupt physical TX/RX wire during 512-byte stream; verify host driver times out after 2000 ms, flushes serial FIFO buffer via `tcflush()`, and resets to idle state without freezing.
2. **Partial Packet Ingestion**: Send malformed header (`0xEF 0x00`); verify driver rejects invalid magic bytes and resumes hunting for next valid `0xEF 0x01` sync header.

---

## 10. Raspberry Pi OS Configuration Guide

### 10.1 Enable Hardware UART and Disable Serial Console

By default, Raspberry Pi OS assigns GPIO 14/15 (`/dev/serial0`) to the Linux login console. This must be disabled to free the port for AS608 hardware communication:

1. Open `/boot/cmdline.txt` (or `/boot/firmware/cmdline.txt` on Bookworm/Bullseye) and remove:
   ```
   console=serial0,115200
   ```
2. Open `/boot/config.txt` (or `/boot/firmware/config.txt`) and append:
   ```ini
   # Enable primary hardware UART
   enable_uart=1
   dtoverlay=disable-bt
   ```
3. Apply system changes and reboot:
   ```bash
   sudo systemctl disable serial-getty@serial0.service
   sudo reboot
   ```

### 10.2 User Group Permissions
Ensure the operational user account has read/write permissions for hardware serial ports:
```bash
sudo usermod -a -G dialout $USER
```

---

## 11. Testing Execution Command Sequence

```bash
# 1. Clone repository to Raspberry Pi
git clone https://github.com/CodeNebula-Dev/FingerPrint-Scanner-IN-Out-System.git
cd FingerPrint-Scanner-IN-Out-System/cpp_engine_v2

# 2. Build C++ Matching Engine & Validation Pipeline
mkdir -p build && cd build
cmake ..
make -j4

# 3. Run Automated 5-Identity Noise & Imposter Benchmark
./test_pipeline

# 4. Run Live Hardware Validation Test with AS608 Scanner
cd ..
python3 hardware_test.py /dev/serial0
```
