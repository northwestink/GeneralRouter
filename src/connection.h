#pragma once
#include "circularbuffer.h"
#include "message.h"

// Represents a single client connection with read/write buffers
class Connection {
public:
  Connection(size_t capacity) : readBuffer(capacity), writeBuffer(capacity) {};
  Message message;
  CircularBuffer readBuffer;  // Buffer for incoming data
  CircularBuffer writeBuffer; // Buffer for outgoing data
};