#ifndef SDIO_H
#define SDIO_H

#include "integer.h"

#define SDIO_DATA     *((volatile BYTE *)0x4060000)
#define SDIO_DATA_32  *((volatile DWORD*)0x4060000)
#define SDIO_SPEED    *((volatile BYTE *)0x4060004)
#define SDIO_CTRL     *((volatile BYTE *)0x4060008)
#define SDIO_CRC      *((volatile BYTE *)0x406000C)
#define SDIO_SWITCH   *((volatile BYTE *)0x4060008)

#define SPI_FORCE_SS 0x01
#define SPI_LEVEL_SS 0x02

#define SD_CARD_DETECT      0x01
#define SD_CARD_PROTECT     0x02
#define SD_TOKEN_SINGLE     0xFE
#define SD_TOKEN_STOP       0xFD
#define SD_TOKEN_MULTI      0xFC
#define SD_RESPONSE_OK      0x05
#define SD_RESPONSE_CRC     0x0B
#define SD_RESPONSE_ERROR   0x0D
#define SD_RESPONSE_TIMEOUT 0xFF


int  sdio_sense(void);
void sdio_init(void);
void sdio_send_command(BYTE, WORD, WORD);
void sdio_set_speed(int);
void sdio_read_block(BYTE *);
BYTE sdio_write_block(const BYTE *, BYTE);

#endif
