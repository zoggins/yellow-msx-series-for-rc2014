/*
 * Copyright 2001-2010 Georges Menie (www.menie.org)
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the University of California, Berkeley nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "xmodem.h"
#include "crc16.h"
#include "serial.h"
#include <stdio.h>

#define SOH   0x01
#define STX   0x02
#define EOT   0x04
#define ACK   0x06
#define NAK   0x15
#define CAN   0x18
#define CTRLZ 0x1A

#define DLY_1S     VDP_FREQUENCY
#define MAXRETRANS 25
#define TRANSMIT_XMODEM_1K

void delay(uint8_t period) __z88dk_fastcall {
  const int16_t timeout = ((int16_t)JIFFY) + period;

  while ((timeout - ((int16_t)JIFFY) >= 0))
    ;
}


static int check(int crc, const unsigned char *buf, int sz) {
  if (crc) {
    const unsigned short crc = crc16_ccitt(buf, sz);
    const unsigned short tcrc = (buf[sz] << 8) + buf[sz + 1];
    return (crc == tcrc);
  }

  int           i;
  unsigned char cks = 0;
  for (i = 0; i < sz; ++i) {
    cks += *buf++;
  }
  return (cks == buf[sz]);
}

static void flush_input(void) {
  while (wait_for_byte((DLY_1S*3) >> 1))
    fossil_rs_in();
}

struct xmodemState xmodemState;

int           crc = 0;
// unsigned char trychar = 'C';
unsigned char packetno = 1;
int           i, c, len = 0;
int           retry, retrans = MAXRETRANS;

bool read_packet() {
  unsigned char *p = xmodemState.packetBuffer;

  *p++ = c;
  for (i = 0; i < (xmodemState.currentPacketSize + (crc) + 3); ++i) {
    if (!wait_for_byte(DLY_1S))
      return false;
    *p++ = fossil_rs_in();
  }

  return true;
}

XMODEM_SIGNAL read_first_header() {
  fossil_rs_out('C');
  crc = 1;

  if (wait_for_byte(DLY_1S)) {
    switch (fossil_rs_in()) {
    case SOH:
      xmodemState.currentPacketSize = 128;
      return READ_128;

    case STX:
      xmodemState.currentPacketSize = 1024;
      return READ_1024;

    case EOT:
      flush_input();
      fossil_rs_out(ACK);
      return END_OF_STREAM;

    case CAN:
      flush_input();
      fossil_rs_out(ACK);
      return UPSTREAM_CANCELLED;
    }
  }

  crc = 0;
  return TRY_AGAIN;
}


XMODEM_SIGNAL read_header() {
  if (wait_for_byte(DLY_1S)) {
    switch (fossil_rs_in()) {
    case SOH:
      xmodemState.currentPacketSize = 128;
      return READ_128;

    case STX:
      xmodemState.currentPacketSize = 1024;
      return READ_1024;

    case EOT:
      flush_input();
      fossil_rs_out(ACK);
      return END_OF_STREAM;

    case CAN:
      flush_input();
      fossil_rs_out(ACK);
      return UPSTREAM_CANCELLED;
    }
  }

  return TRY_AGAIN;
}

XMODEM_SIGNAL start_receive() {
  if (!read_packet())
    return PACKET_REJECT;

  if (xmodemState.packetBuffer[1] == (unsigned char)(~xmodemState.packetBuffer[2]) && (xmodemState.packetBuffer[1] == packetno) && check(crc, &xmodemState.packetBuffer[3], xmodemState.currentPacketSize))
    return SAVE_PACKET;

  if (--retrans <= 0) {
    flush_input();
    fossil_rs_out(CAN);
    fossil_rs_out(CAN);
    fossil_rs_out(CAN);
    return TOO_MANY_ERRORS;
  }

  return PACKET_REJECT;
}

XMODEM_SIGNAL xmodem_receive(XMODEM_SIGNAL signal) __z88dk_fastcall {
  switch(signal) {
    case READ_FIRST_HEADER:
      return read_first_header();

    case READ_HEADER:
      return read_header();

    case READ_128:
    case READ_1024:
      return start_receive();

    case PACKET_REJECT:
    case TRY_AGAIN:
      flush_input();

      if (retry++ >= 16)
        return TOO_MANY_ERRORS;

      delay(DLY_1S*4);

      printf("w");
      fossil_rs_out(NAK);
      retry++;
      return READ_HEADER;

    case SAVE_PACKET:
      ++packetno;
      retrans = MAXRETRANS + 1;
      retry = 0;
      fossil_rs_out(ACK);
      return READ_HEADER;

    // case END_OF_STREAM:
    // case UPSTREAM_CANCELLED:
    // case STREAM_ERROR:
    case TOO_MANY_ERRORS:
      flush_input();
      fossil_rs_out(CAN);
      fossil_rs_out(CAN);
      fossil_rs_out(CAN);
      xmodemState.finish_reason = TOO_MANY_ERRORS;
      return FINISHED;

    default:
      xmodemState.finish_reason = signal;
      return FINISHED;
  }
}

// int xmodemTransmit()
// {
//   const int srcsz = 10;

// 	unsigned char xmodemState.packetBuffer[1030]; /* 1024 for XModem 1k + 3 head chars + 2 crc + nul */
// 	int xmodemState.currentPacketSize, crc = -1;
// 	unsigned char packetno = 1;
// 	int i, c, len = 0;
// 	int retry;

// 	for(;;) {
// 		for( retry = 0; retry < 16; ++retry) {
// 			if ((c = _inbyte((DLY_1S)<<1)) >= 0) {
// 				switch (c) {
// 				case 'C':
// 					crc = 1;
// 					goto start_trans;
// 				case NAK:
// 					crc = 0;
// 					goto start_trans;
// 				case CAN:
// 					if ((c = _inbyte(DLY_1S)) == CAN) {
// 						fossil_rs_out(ACK);
// 						flush_input();
// 						return -1; /* canceled by remote */
// 					}
// 					break;
// 				default:
// 					break;
// 				}
// 			}
// 		}
// 		fossil_rs_out(CAN);
// 		fossil_rs_out(CAN);
// 		fossil_rs_out(CAN);
// 		flush_input();
// 		return -2; /* no sync */

// 		for(;;) {
// 		start_trans:
// #ifdef TRANSMIT_XMODEM_1K
// 			xmodemState.packetBuffer[0] = STX; xmodemState.currentPacketSize = 1024;
// #else
// 			xmodemState.packetBuffer[0] = SOH; xmodemState.currentPacketSize = 128;
// #endif
// 			xmodemState.packetBuffer[1] = packetno;
// 			xmodemState.packetBuffer[2] = ~packetno;
// 			c = srcsz - len;
// 			if (c > xmodemState.currentPacketSize) c = xmodemState.currentPacketSize;
// 			if (c > 0) {
// 				// memset (&xmodemState.packetBuffer[3], 0, xmodemState.currentPacketSize);
// 				// memcpy (&xmodemState.packetBuffer[3], &src[len], c);
// 				if (c < xmodemState.currentPacketSize) xmodemState.packetBuffer[3+c] = CTRLZ;
// 				if (crc) {
// 					unsigned short ccrc = crc16_ccitt(&xmodemState.packetBuffer[3], xmodemState.currentPacketSize);
// 					xmodemState.packetBuffer[xmodemState.currentPacketSize+3] = (ccrc>>8) & 0xFF;
// 					xmodemState.packetBuffer[xmodemState.currentPacketSize+4] = ccrc & 0xFF;
// 				}
// 				else {
// 					unsigned char ccks = 0;
// 					for (i = 3; i < xmodemState.currentPacketSize+3; ++i) {
// 						ccks += xmodemState.packetBuffer[i];
// 					}
// 					xmodemState.packetBuffer[xmodemState.currentPacketSize+3] = ccks;
// 				}
// 				for (retry = 0; retry < MAXRETRANS; ++retry) {
// 					for (i = 0; i < xmodemState.currentPacketSize+4+(crc?1:0); ++i) {
// 						fossil_rs_out(xmodemState.packetBuffer[i]);
// 					}
// 					if ((c = _inbyte(DLY_1S)) >= 0 ) {
// 						switch (c) {
// 						case ACK:
// 							++packetno;
// 							len += xmodemState.currentPacketSize;
// 							goto start_trans;
// 						case CAN:
// 							if ((c = _inbyte(DLY_1S)) == CAN) {
// 								fossil_rs_out(ACK);
// 								flush_input();
// 								return -1; /* canceled by remote */
// 							}
// 							break;
// 						case NAK:
// 						default:
// 							break;
// 						}
// 					}
// 				}
// 				fossil_rs_out(CAN);
// 				fossil_rs_out(CAN);
// 				fossil_rs_out(CAN);
// 				flush_input();
// 				return -4; /* xmit error */
// 			}
// 			else {
// 				for (retry = 0; retry < 10; ++retry) {
// 					fossil_rs_out(EOT);
// 					if ((c = _inbyte((DLY_1S)<<1)) == ACK) break;
// 				}
// 				flush_input();
// 				return (c == ACK)?len:-5;
// 			}
// 		}
// 	}
// }