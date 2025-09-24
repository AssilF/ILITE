#pragma once

#include <Arduino.h>

// Initializes the connection log system. Currently a no-op but provided for completeness.
void connectionLogInit();

// Adds a message to the log (truncated if necessary).
void connectionLogAdd(const char* message);

// Adds a formatted message to the log using printf-style formatting.
void connectionLogAddf(const char* fmt, ...);

// Returns the number of log entries currently stored.
size_t connectionLogGetCount();

// Returns a pointer to the log entry at the provided index (0 = oldest).
// Returns nullptr if the index is out of range.
const char* connectionLogGetEntry(size_t index);

// Clears all stored log entries.
void connectionLogClear();
