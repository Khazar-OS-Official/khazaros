#ifndef RTC_H
#define RTC_H

#include <libk/types.h>

#define RTC_SECOND  0x00
#define RTC_MINUTE  0x02
#define RTC_HOUR    0x04
#define RTC_DAY     0x07
#define RTC_MONTH   0x08
#define RTC_YEAR    0x09
#define RTC_STATUS_B 0x0B

struct rtc_time {
  uint8_t second;
  uint8_t minute;
  uint8_t hour;
  uint8_t day;
  uint8_t month;
  uint16_t year;
  uint8_t weekday;
};

void rtc_init(void);
void rtc_read(struct rtc_time *out);

#endif
