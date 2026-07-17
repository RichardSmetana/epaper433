#ifndef DBG_H
#define DBG_H

// Set to 0 to disable all Serial output (no Serial.begin, smaller/faster)
#ifndef SERIAL_DEBUG
#define SERIAL_DEBUG 1
#endif

#if SERIAL_DEBUG
#define DBG_BEGIN(baud) Serial.begin(baud)
#define DBG_PRINT(...)  Serial.print(__VA_ARGS__)
#define DBG_PRINTLN(...) Serial.println(__VA_ARGS__)
#else
#define DBG_BEGIN(baud) ((void)0)
#define DBG_PRINT(...)  ((void)0)
#define DBG_PRINTLN(...) ((void)0)
#endif

#endif
