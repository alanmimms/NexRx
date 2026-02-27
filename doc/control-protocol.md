# NexRx Control Protocol Specification

This document defines the communication protocol used for control and
configuration between the NexRx Application (Client) and the NexRx
Digital Twin or Physical Hardware (Server). This protocol is only for
the _control plane:_ It doesn't carry digital media streaming data. It
gives the NexRx host app control over the hardware and software in the
NexRx radio.

## 1. Overview

The control plane uses a reliable, bi-directional stream for sending
commands and receiving responses. It is designed to be
transport-agnostic, though currently implemented over TCP/IP for use
with the digital twin.

*   **Default TCP Port:** 5000
*   **Role:** The App is the Client; the Twin/Hardware is the Server.
*   **Model:** Synchronous Request-Response using CBOR.

## 2. Transport Layer

The protocol runs over TCP. To ensure low-latency performance, the
`TCP_NODELAY` option (Nagle's algorithm disabled) is enabled on both
sides.

## 3. Framing

Every message (request and response) is encapsulated in a binary frame.

### 3.1 Frame Structure

| Offset | Size | Type | Description |
| :--- | :--- | :--- | :--- |
| 0 | Var | CBOR uint | **Payload Length**: The number of bytes in the following payload, encoded as a CBOR unsigned integer. |
| Var | N | bytes | **Payload**: The CBOR-encoded message data. |

*   **Byte Order:** CBOR encoding naturally handles byte order (network byte order).

## 4. Message Format (CBOR)

All payloads are encoded using **CBOR (Concise Binary Object Representation)**.

### 4.1 Request Format

A request is a CBOR array:
`[command_id, [args...]]`

*   **command_id**: A 4-byte text string identifier.
*   **args**: An array of arguments specific to the command.

### 4.2 Response Format

A response is a CBOR array:
`[status_code, payload]`

*   **status_code**: 0 for success (`OK`), non-zero for errors.
*   **payload**: Command-specific return data (e.g., a string, a nested array, or a JSON string).

## 5. Command Reference

### 5.1 System Control

| Command | ID (String) | Args | Description |
| :--- | :--- | :--- | :--- |
| `START_STREAM` | `STM[` | `[]` | Enables IQ data streaming via the UDP data plane. |
| `STOP_STREAM` | `]STM` | `[]` | Disables IQ data streaming. |
| `GET_STATUS` | `GSTS` | `[]` | Returns a JSON string containing system status. |
| `GET_CONFIGURATION`| `GCNF` | `[]` | Returns a JSON string describing hardware capabilities. |
| `DISCONNECT` | `GBYE` | `[]` | Hint from client that it is closing the connection. |

### 5.2 Receiver Configuration

| Command | ID (String) | Args | Description |
| :--- | :--- | :--- | :--- |
| `SET_QSD_VFO` | `SVFO` | `[index, freq_hz]` | Sets the NCO frequency for QSD 0, 1, or 2. |
| `SET_ATTEN` | `SATT` | `[db_value, enabled]` | Enables/disables a specific attenuator stage (3, 6, 12, or 24 dB). |
| `SET_PRESEL_C` | `SPRC` | `[index, enabled]` | Enables/disables one of the 11 preselector capacitors. |
| `SET_PRESEL_L` | `SPRL` | `[index, enabled]` | Enables/disables a preselector inductor (index 0 for L701 bypass). |

### 5.3 Internal Signal Generator (ISG)

The Internal Signal Generator (formerly BIST) provides a reference signal for calibration and testing.

| Command | ID (String) | Args | Description |
| :--- | :--- | :--- | :--- |
| `SET_ISG_ENABLE`| `SIEN` | `[enabled]` | Enables/disables the ISG. |
| `SET_ISG_FREQ` | `SIFQ` | `[freq_hz]` | Sets the frequency of the ISG signal. |

### 5.4 Calibration

Calibration data is stored as JSON strings.

| Command | ID (String) | Args | Description |
| :--- | :--- | :--- | :--- |
| `SET_CALIBRATION`| `SCAL` | `[type, json_data]` | Stores a JSON calibration string for a specific type. |
| `GET_CALIBRATION`| `GCAL` | `[type]` | Retrieves a JSON calibration string. |

### 5.5 Audio Codec (AK5578)

Configures the AK5578 audio codec parameters.

| Command | ID (String) | Args | Description |
| :--- | :--- | :--- | :--- |
| `SET_CODEC` | `SCOD` | `[rate, channels, gain, lpf]` | Configures sample rate, channels, gain, and LPF. |

## 6. Philosophy

The philosophy for the attenuator and the L and C components in the
preselector is to provide raw calibration and hardware capability data
to the application and have the application determine which specific
components to enable to achieve its goals.

This ensures that if complex calculations are required—for example,
combining calibration data with DSP signal metrics to determine the
optimal L/C/Attenuator configuration—these calculations are performed
on the host application where significantly more compute power is
available compared to the embedded controller.

## 7. Data Plane (UDP)

High-bandwidth IQ sample data is sent over a separate UDP stream
(default port 5001).

### 7.1 Protocol Structure (CBOR)

The data plane uses **CBOR** for efficient serialization. Every UDP
packet is a CBOR-encoded array:

`["NXRQ", version, type, frames]`

*   **Magic String:** `"NXRQ"`
*   **Version:** `1`
*   **Type:** 
    *   `0`: `TYPE_IQ_DATA` (Server streaming data)
    *   `1`: `TYPE_TX_AUDIO` (Future: Client-to-server transmit audio)

### 7.2 IQFrame Structure

Each frame in the `frames` array is a CBOR array:

`[sequence, timestamp_ns, i0, q0, i1, q1, i2, q2]`

*   **sequence:** 32-bit unsigned integer.
*   **timestamp_ns:** 64-bit unsigned integer.
*   **iN, qN:** 24-bit signed integers.
