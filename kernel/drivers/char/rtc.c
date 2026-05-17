#include <drivers/rtc.h>
#include <arch/io.h>
#include <drivers/vga.h>

#define CMOS_ADDR  0x70
#define CMOS_DATA  0x71

static uint8_t rtc_bcd_to_bin(uint8_t bcd) {
  return (bcd >> 4) * 10 + (bcd & 0x0F);
}

static uint8_t rtc_read_reg(uint8_t reg) {
  outb(CMOS_ADDR, 0x80 | reg); /* NMI disable */
  return inb(CMOS_DATA);
}

void rtc_init(void) {
  kprintf("RTC: Initializing... ");
  /* RTC is always present on PC; no init needed */
  kprintf("OK\n");
}

void rtc_read(struct rtc_time *out) {
  if (!out) return;

  uint8_t status_b = rtc_read_reg(RTC_STATUS_B);
  int use_24h = (status_b & 0x02) ? 1 : 0;

  out->second = rtc_read_reg(RTC_SECOND);
  out->minute = rtc_read_reg(RTC_MINUTE);
  out->hour   = rtc_read_reg(RTC_HOUR);
  out->day    = rtc_read_reg(RTC_DAY);
  out->month  = rtc_read_reg(RTC_MONTH);
  out->year   = (uint16_t)rtc_read_reg(RTC_YEAR);
  out->weekday = rtc_read_reg(0x06);

  /* BCD to binary */
  if (!(status_b & 0x04)) {
    out->second = rtc_bcd_to_bin(out->second);
    out->minute = rtc_bcd_to_bin(out->minute);
    out->hour   = rtc_bcd_to_bin(out->hour);
    out->day    = rtc_bcd_to_bin(out->day);
    out->month  = rtc_bcd_to_bin(out->month);
    out->year   = (uint16_t)rtc_bcd_to_bin((uint8_t)out->year);
  }

  /* 12h -> 24h */
  if (!use_24h && (out->hour & 0x80)) {
    out->hour = ((out->hour & 0x7F) + 12) % 24;
  }
  out->hour &= 0x7F;

  /* Year in 20xx */
  if (out->year < 100)
    out->year += 2000;
}
