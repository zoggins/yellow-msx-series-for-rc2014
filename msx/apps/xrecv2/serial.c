#include "serial.h"
#include "msxdos.h"
#include <stdio.h>
#include <system_vars.h>

bool wait_for_byte(uint16_t period) __z88dk_fastcall {
  const int16_t timeout = ((int16_t)JIFFY) + period;

  while ((timeout - ((int16_t)JIFFY) >= 0) && !fossil_rs_in_stat())
    if (msxbiosBreakX()) {
      return false;
    }

  return fossil_rs_in_stat();
}

void fossil_flush_input(uint16_t period) __z88dk_fastcall {
  while (wait_for_byte(period))
    fossil_rs_in();
}
