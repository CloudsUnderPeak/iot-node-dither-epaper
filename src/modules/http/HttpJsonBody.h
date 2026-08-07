#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "api/shared/ApiTypes.h"

namespace HttpJsonBody {

constexpr size_t kMaxBytes = 2048;

// Validates the HTTP JSON transport contract and deserializes a bounded body.
// A successful response means `document` contains the parsed request value.
Api::Response parse(const char *contentType,
                    const uint8_t *body,
                    size_t bodyLength,
                    JsonDocument &document);

}  // namespace HttpJsonBody
