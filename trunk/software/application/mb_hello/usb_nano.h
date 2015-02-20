/*
 * usb_new.h
 *
 *  Created on: Feb 4, 2015
 *      Author: Gideon
 */

#ifndef USB_NANO_H_
#define USB_NANO_H_

#include "itu.h"
#include "iomap.h"

#define USB2_BASE    USB_BASE
#define NANO_BASE    USB_BASE

#define USB2_CMD_Command  (*(volatile WORD *)(NANO_BASE + 0x7E0))
#define USB2_CMD_DevEP    (*(volatile WORD *)(NANO_BASE + 0x7E2))
#define USB2_CMD_Length   (*(volatile WORD *)(NANO_BASE + 0x7E4))
#define USB2_CMD_MaxTrans (*(volatile WORD *)(NANO_BASE + 0x7E6))
#define USB2_CMD_MemHi    (*(volatile WORD *)(NANO_BASE + 0x7E8))
#define USB2_CMD_MemLo    (*(volatile WORD *)(NANO_BASE + 0x7EA))
#define USB2_CMD_SplitCtl (*(volatile WORD *)(NANO_BASE + 0x7EC))
#define USB2_CMD_Result   (*(volatile WORD *)(NANO_BASE + 0x7EE))

#define ATTR_FIFO_BASE    (NANO_BASE + 0x640)
#define BLOCK_FIFO_BASE	  (NANO_BASE + 0x740)
#define ATTR_FIFO_ENTRIES  128
#define BLOCK_FIFO_ENTRIES 64

#define USB2_CIRC_MEMADDR_START_HI  (*(volatile WORD *)(NANO_BASE + 0x7C0))
#define USB2_CIRC_MEMADDR_START_LO  (*(volatile WORD *)(NANO_BASE + 0x7C2))
#define USB2_CIRC_BUF_SIZE		  	(*(volatile WORD *)(NANO_BASE + 0x7C4))
#define USB2_CIRC_BUF_OFFSET		(*(volatile WORD *)(NANO_BASE + 0x7C6))

#define USB2_BLOCK_BASE_HI	(*(volatile WORD *)(NANO_BASE + 0x7C8))
#define USB2_BLOCK_BASE_LO	(*(volatile WORD *)(NANO_BASE + 0x7CA))

#define USB2_STATUS 	    (*(volatile BYTE *)(NANO_BASE + 0x7CD)) // low byte of 3E6

#define ATTR_FIFO_TAIL		(*(volatile WORD *)(NANO_BASE + 0x7F0)) // updated by software only
#define ATTR_FIFO_HEAD		(*(volatile WORD *)(NANO_BASE + 0x7F2)) // updated by nano only
#define BLOCK_FIFO_TAIL		(*(volatile WORD *)(NANO_BASE + 0x7F4)) // updated by nano only
#define BLOCK_FIFO_HEAD		(*(volatile WORD *)(NANO_BASE + 0x7F6)) // updated by software only

#define USB2_NUM_PIPES	8
#define USB2_PIPES_BASE (NANO_BASE + 0x5C0)
#define USB2_PIPE_SIZE  (0x10)

#define PIPE_OFFS_Command    0 // note: WORD offsets!
#define PIPE_OFFS_DevEP      1
#define PIPE_OFFS_Length     2
#define PIPE_OFFS_MaxTrans   3
#define PIPE_OFFS_Interval   4
#define PIPE_OFFS_LastFrame  5
#define PIPE_OFFS_SplitCtl   6
#define PIPE_OFFS_Result     7

struct t_pipe {
	WORD Command;
	WORD DevEP;
	WORD Length;
	WORD MaxTrans;
	WORD Interval;
	WORD SplitCtl;
};

#define NANO_DO_RESET	    (*(volatile BYTE *)(NANO_BASE + 0x7FB))
#define NANO_DO_SUSPEND	    (*(volatile BYTE *)(NANO_BASE + 0x7F9))
#define NANO_LINK_SPEED	    (*(volatile BYTE *)(NANO_BASE + 0x7FD))
#define NANO_SIMULATION	    (*(volatile BYTE *)(NANO_BASE + 0x7FF))
#define NANO_START  		(*(volatile BYTE *)(NANO_BASE + 0x800))

// Bit definitions
#define USTAT_CONNECTED		0x01
#define USTAT_OPERATIONAL	0x02
#define USTAT_SUSPENDED	    0x04

#define UCMD_MEMREAD      0x8000
#define UCMD_MEMWRITE	  0x4000
#define UCMD_USE_BLOCK    0x2000
#define UCMD_USE_CIRC     0x1000
#define UCMD_TOGGLEBIT	  0x0800
#define UCMD_RETRY_ON_NAK 0x0400
#define UCMD_DO_SPLIT	  0x0080
#define UCMD_DO_DATA	  0x0040
#define UCMD_SETUP		  0x0000
#define UCMD_OUT		  0x0001
#define UCMD_IN			  0x0002

#define SPLIT_START		0x0000
#define SPLIT_COMPLETE	0x0080
#define SPLIT_SPEED		0x0040
#define SPLIT_CONTROL   0x0000
#define SPLIT_ISOCHRO   0x0010
#define SPLIT_BULK      0x0020
#define SPLIT_INTERRUPT 0x0030
#define SPLIT_PORT_MSK  0x000F
#define SPLIT_HUBAD_MSK 0x7F00

#define URES_DONE	    0x8000
#define URES_PACKET	    0x0000
#define URES_ACK	    0x1000
#define URES_NAK	    0x2000
#define URES_NYET	    0x3000
#define URES_STALL      0x4000
#define URES_ERROR	    0x5000
#define URES_RESULT_MSK 0x7000
#define URES_TOGGLE     0x0800
#define URES_NO_DATA    0x0400
#define URES_LENGTH_MSK 0x03FF

#endif /* USB_NANO_H_ */
