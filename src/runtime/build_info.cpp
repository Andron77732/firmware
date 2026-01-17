#include "runtime/build_info.h"

#include <cstdio>
#include <cstring>

namespace {

int monthFromAbbrev(const char* month) {
  static const char* kMonths[] = {
      "Jan", "Feb", "Mar", "Apr", "May", "Jun",
      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
  };

  for (int i = 0; i < 12; ++i) {
    if (std::strncmp(month, kMonths[i], 3) == 0) {
      return i + 1;
    }
  }

  return 0;
}

int parseDay(const char* date) {
  const char tens = date[4];
  const char ones = date[5];
  if (tens == ' ') {
    return ones - '0';
  }
  return (tens - '0') * 10 + (ones - '0');
}

int parseYear(const char* date) {
  return (date[7] - '0') * 1000 +
         (date[8] - '0') * 100 +
         (date[9] - '0') * 10 +
         (date[10] - '0');
}

}  // namespace

const char* firmware_build_date_iso() {
  static char iso_date[11] = {0};
  static bool initialized = false;

  if (!initialized) {
    const char* date = __DATE__;
    int month = monthFromAbbrev(date);
    int day = parseDay(date);
    int year = parseYear(date);

    if (month <= 0 || day <= 0 || year <= 0) {
      std::snprintf(iso_date, sizeof(iso_date), "0000-00-00");
    } else {
      std::snprintf(iso_date, sizeof(iso_date), "%04d-%02d-%02d", year, month, day);
    }

    initialized = true;
  }

  return iso_date;
}
