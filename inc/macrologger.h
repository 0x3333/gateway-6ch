/*
 * Copyright (c) 2012 David Rodrigues
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef __MACROLOGGER_H__
#define __MACROLOGGER_H__

#include <stdio.h>
#include <string.h>
#include <FreeRTOS.h>
#include <task.h>

// === auxiliar functions
static inline char *timenow()
{
    static char buffer[36];

    TickType_t ticks = xTaskGetTickCount();
    snprintf(buffer, sizeof(buffer), "%7lu", ticks);

    return buffer;
}

#define _FILE strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__

#define NO_LOG 0x00
#define ERROR_LEVEL 0x01
#define INFO_LEVEL 0x02
#define DEBUG_LEVEL 0x03

#ifndef LOG_LEVEL
#define LOG_LEVEL DEBUG_LEVEL
#endif

#define PRINTFUNCTION(format, ...) printf(format, __VA_ARGS__)

#define LOG_FMT "%s | %-7s | %15s:%3d | "
#define LOG_ARGS(LOG_TAG) timenow(), LOG_TAG, _FILE, __LINE__

#define NEWLINE "\n"

#define ERROR_TAG "ERROR"
#define INFO_TAG "INFO"
#define DEBUG_TAG "DEBUG"

#define LOG_BOOL(b) ((b) ? 'Y' : 'N')

#define DEV_FMT "B:%u S:%u A:%u | "
#define DEVF_FMT "B:%u S:%u A:%u F:%u | "

#if LOG_LEVEL >= DEBUG_LEVEL
#define LOG_DEBUG(message, ...) PRINTFUNCTION(LOG_FMT message NEWLINE, LOG_ARGS(DEBUG_TAG), ##__VA_ARGS__)
#define LOG_DEBUGD(message, dev, ...) PRINTFUNCTION(LOG_FMT DEV_FMT message NEWLINE, LOG_ARGS(DEBUG_TAG), (dev)->bus, (dev)->slave, (dev)->address, ##__VA_ARGS__)
#define LOG_DEBUGDF(message, dev, ...) PRINTFUNCTION(LOG_FMT DEVF_FMT message NEWLINE, LOG_ARGS(DEBUG_TAG), (dev)->bus, (dev)->slave, (dev)->address, (dev)->function, ##__VA_ARGS__)
#else
#define LOG_DEBUG(message, ...)
#define LOG_DEBUGD(message, dev, ...)
#define LOG_DEBUGDF(message, dev, ...)
#endif

#if LOG_LEVEL >= INFO_LEVEL
#define LOG_INFO(message, ...) PRINTFUNCTION(LOG_FMT message NEWLINE, LOG_ARGS(INFO_TAG), ##__VA_ARGS__)
#define LOG_INFOD(message, dev, ...) PRINTFUNCTION(LOG_FMT DEV_FMT message NEWLINE, LOG_ARGS(INFO_TAG), (dev)->bus, (dev)->slave, (dev)->address, ##__VA_ARGS__)
#define LOG_INFODF(message, dev, ...) PRINTFUNCTION(LOG_FMT DEVF_FMT message NEWLINE, LOG_ARGS(INFO_TAG), (dev)->bus, (dev)->slave, (dev)->address, (dev)->function, ##__VA_ARGS__)
#else
#define LOG_INFO(message, ...)
#define LOG_INFOD(message, dev, ...)
#define LOG_INFODF(message, dev, ...)
#endif

#if LOG_LEVEL >= ERROR_LEVEL
#define LOG_ERROR(message, ...) PRINTFUNCTION(LOG_FMT message NEWLINE, LOG_ARGS(ERROR_TAG), ##__VA_ARGS__)
#define LOG_ERRORD(message, dev, ...) PRINTFUNCTION(LOG_FMT DEV_FMT message NEWLINE, LOG_ARGS(ERROR_TAG), (dev)->bus, (dev)->slave, (dev)->address, ##__VA_ARGS__)
#define LOG_ERRORDF(message, dev, ...) PRINTFUNCTION(LOG_FMT DEVF_FMT message NEWLINE, LOG_ARGS(ERROR_TAG), (dev)->bus, (dev)->slave, (dev)->address, (dev)->function, ##__VA_ARGS__)
#else
#define LOG_ERROR(message, ...)
#define LOG_ERRORD(message, dev, ...)
#define LOG_ERRORDF(message, dev, ...)
#endif

#if LOG_LEVEL >= NO_LOGS
#define LOG_IF_ERROR(condition, message, ...) \
    if (condition)                            \
    PRINTFUNCTION(LOG_FMT message NEWLINE, LOG_ARGS(ERROR_TAG), ##__VA_ARGS__)
#else
#define LOG_IF_ERROR(condition, message, ...)
#endif

#endif