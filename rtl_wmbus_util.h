#include <stddef.h>
#include <time.h>

#if WINDOWS_BUILD
#else
#include <sys/time.h>
#endif


inline int make_time_string(char* timestamp, size_t timestamp_size)
{
  memset(timestamp, 0, timestamp_size);

#if WINDOWS_BUILD
  time_t now;
  if (time(&now) == NULL)
	return -1;

  struct tm* timeinfo = gmtime(&now);
  if (timeinfo == NULL)
	return -1;

  strftime(timestamp, timestamp_size, "%Y-%m-%d %H:%M:%S.000000", timeinfo);
#else
  struct timeval tv;
  if (gettimeofday(&tv, NULL) != 0)
	return -1;

  struct tm timeinfo;
  if (localtime_r(&tv.tv_sec, &timeinfo) == NULL)
	return -1;

  char fmt[timestamp_size];
  strftime(fmt, sizeof(fmt), "%Y-%m-%d %H:%M:%S.%%06u", &timeinfo);
  snprintf(timestamp, timestamp_size, fmt, tv.tv_usec);
#endif

  return 0;
}
