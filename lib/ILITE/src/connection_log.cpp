#include "connection_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace {
constexpr size_t kMaxEntries = 32;
constexpr size_t kEntryLength = 48;

char logEntries[kMaxEntries][kEntryLength] = {};
size_t logCount = 0;
size_t nextIndex = 0;
bool loggingEnabled = false;

size_t oldestIndex() {
  if (logCount == 0) {
    return 0;
  }
  if (logCount < kMaxEntries) {
    return 0;
  }
  return nextIndex;
}

void writeEntry(const char* text) {
  if (!text) {
    return;
  }
  strncpy(logEntries[nextIndex], text, kEntryLength - 1);
  logEntries[nextIndex][kEntryLength - 1] = '\0';
  nextIndex = (nextIndex + 1) % kMaxEntries;
  if (logCount < kMaxEntries) {
    ++logCount;
  }
}

}  // namespace

void connectionLogInit() {
  connectionLogClear();
}

void connectionLogAdd(const char* message) {
  if (!loggingEnabled) {
    return;
  }
  if (!message || message[0] == '\0') {
    return;
  }
  writeEntry(message);
}

void connectionLogAddf(const char* fmt, ...) {
  if (!loggingEnabled) {
    return;
  }
  if (!fmt) {
    return;
  }
  char buffer[kEntryLength];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  writeEntry(buffer);
}

size_t connectionLogGetCount() {
  return logCount;
}

const char* connectionLogGetEntry(size_t index) {
  if (index >= logCount) {
    return nullptr;
  }
  size_t start = oldestIndex();
  size_t actual = (start + index) % kMaxEntries;
  return logEntries[actual];
}

void connectionLogClear() {
  for (size_t i = 0; i < kMaxEntries; ++i) {
    logEntries[i][0] = '\0';
  }
  logCount = 0;
  nextIndex = 0;
}

void connectionLogSetRecordingEnabled(bool enabled) {
  loggingEnabled = enabled;
}

bool connectionLogIsRecordingEnabled() {
  return loggingEnabled;
}
