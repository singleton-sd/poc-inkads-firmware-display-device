#pragma once

#include <Arduino.h>

class EntraHttpsClient {
 public:
  bool get(const char* url, char* body, size_t bodySize, size_t* bodyLength,
           int* statusCode);
  bool postForm(const char* url, const char* form, char* body, size_t bodySize,
                size_t* bodyLength, int* statusCode);

 private:
  bool perform(const char* url, const char* form, char* body, size_t bodySize,
               size_t* bodyLength, int* statusCode);
};
