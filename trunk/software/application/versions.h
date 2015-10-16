#ifndef VERSIONS_H
#define VERSIONS_H

// 1 = corner lower right
// 2 = horizontal bar
// 3 = corner lower left
// 4 = vertical bar
// 5 = corner upper right
// 6 = corner upper left
// 7 = rounded corner lower right
// 8 = T
// 9 = rounded corner lower left
// A = |-
// B = +
// C = -|
// D = rounded corner upper right
// E = _|_
// F = rounded corner upper left
// 10 = alpha
// 11 = beta

// alpha = \020
// beta  = \021

#define APPL_VERSION "3.0\x11\x35"
#define BOOT_VERSION "V3.1"
#define FPGA_VERSION "FPGA U2 V104"
#define MINIMUM_FPGA_REV 0x01

#endif

/* note: 'c' is to avoid confusion with 'b' of beta. */
