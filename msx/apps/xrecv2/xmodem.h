#ifndef __XMODEM
#define __XMODEM

#include <stdlib.h>

typedef enum {
  FINISHED = 0,
  READ_FIRST_HEADER = 0x0001,
  READ_HEADER = 0x0002,
  READ_CRC = 0x0004,
  READ_CHECKSUM = 0x0008,
  READ_128 = 0x0010,
  READ_1024 = 0x0020,
  SAVE_PACKET = 0x0040,
  END_OF_STREAM = 0x0080,
  UPSTREAM_CANCELLED = 0x0100,
  STREAM_ERROR = 0x0200,
  TRY_AGAIN = 0x0400,
  PACKET_REJECT = 0x0800,
  TOO_MANY_ERRORS = 0x1000,
  PACKET_TIMEOUT = 0x2000
} XMODEM_SIGNAL;

// _128, READ_CRC_1024, READ_128, READ_1024, SAVE_PACKET, END_OF_STREAM, UPSTREAM_CANCELLED, STREAM_ERROR, TRY_AGAIN, PACKET_REJECT, TOO_MANY_ERRORS } XMODEM_SIGNAL;

extern XMODEM_SIGNAL xmodem_receive(XMODEM_SIGNAL signal) __z88dk_fastcall;

typedef struct xmodemState {
  unsigned char packetBuffer[1030];
  uint16_t      currentPacketSize;
  XMODEM_SIGNAL finish_reason;
};

extern struct xmodemState xmodemState;

#endif