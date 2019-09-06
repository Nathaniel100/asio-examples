#ifndef CHAT_MESSAGE_H
#define CHAT_MESSAGE_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

class ChatMessage {
 public:
  enum { HEADER_LENGTH = 4 };
  enum { MAX_MESSAGE_LENGTH = 512 };

  const char *body() const { return data_ + HEADER_LENGTH; }

  char *body() { return data_ + HEADER_LENGTH; }

  const char *data() const { return data_; }

  char *data() { return data_; }

  size_t body_length() const { return body_length_; }

  void body_length(size_t new_length) {
    body_length_ = new_length;
    if (body_length_  > MAX_MESSAGE_LENGTH) {
      body_length_ = MAX_MESSAGE_LENGTH;
    }
  }

  size_t length() const { return HEADER_LENGTH + body_length_; }

  void encode_header() {
    char header[HEADER_LENGTH + 1] = {0};
    snprintf(header, sizeof(header), "%4d", static_cast<int>(body_length_));
    memcpy(data_, header, HEADER_LENGTH);
  }

  bool decode_header() {
    char header[HEADER_LENGTH + 1] = {0};
    strncat(header, data_, HEADER_LENGTH);
    body_length_ = strtoul(data_, nullptr, 0);
    if (body_length_ > MAX_MESSAGE_LENGTH) {
      body_length_ = 0;
      return false;
    }
    return true;
  }

 private:
  size_t body_length_ = 0;
  char data_[HEADER_LENGTH + MAX_MESSAGE_LENGTH];
};

#endif
