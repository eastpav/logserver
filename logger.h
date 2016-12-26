#ifndef __LOGGER_H__
#define __LOGGER_H__

#define logInfo(fmt, args...)  printf(fmt, ##args)

#define logErr(fmt, args...)  printf(fmt, ##args)

#define logWarn(fmt, args...)  printf(fmt, ##args)

#define logDebug(fmt, args...)  printf(fmt, ##args)

#endif

