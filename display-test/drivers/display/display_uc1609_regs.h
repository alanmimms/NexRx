/*
 * Copyright (c) 2026 Starview Labs (starviewlabs.com)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __DISPLAY_UC1609_REGS_H__
#define __DISPLAY_UC1609_REGS_H__

/* All following bytes will contain commands */
#define UC1609_CONTROL_ALL_BYTES_CMD     0x00
/* All following bytes will contain data */
#define UC1609_CONTROL_ALL_BYTES_DATA    0x40
/* The next byte will contain a command, then expect another control byte */
#define UC1609_CONTROL_CONTINUE_BYTE_CMD 0x80

#define UC1609_DISPLAY_OFF              0xAE
#define UC1609_DISPLAY_ON               0xAF
#define UC1609_DISPLAY_ALL_PIXEL_ON     0xA5
#define UC1609_DISPLAY_ALL_PIXEL_NORMAL 0xA4
#define UC1609_SET_CONTRAST_CTRL        0x81
#define UC1609_SET_BIAS                 0xA2
#define UC1609_SET_REGULATION_RATIO     0x20

/* Inversion controls */
#define UC1609_SET_NORMAL_DISPLAY  0xA6
#define UC1609_SET_REVERSE_DISPLAY 0xA7

/* COM invdir */
#define UC1609_SET_COM_OUTPUT_SCAN_FLIPPED 0xC8
#define UC1609_SET_COM_OUTPUT_SCAN_NORMAL  0xC0

/* SEG invdir */
#define UC1609_SET_SEGMENT_MAP_FLIPPED 0xA1
#define UC1609_SET_SEGMENT_MAP_NORMAL  0xA0

/* Power Control */
#define UC1609_POWER_CONTROL    0x28
#define UC1609_POWER_CONTROL_VF 0x1
#define UC1609_POWER_CONTROL_VR 0x2
#define UC1609_POWER_CONTROL_VB 0x4

/* Offsets */
#define UC1609_COLUMN_MSB  0x10
#define UC1609_COLUMN_LSB  0x0
#define UC1609_PAGE        0xB0
#define UC1609_LINE_SCROLL 0x40

#define UC1609_RESET_DELAY 1
#define UC1609_RESET       0xE2

#endif /* __DISPLAY_UC1609_REGS_H__ */
