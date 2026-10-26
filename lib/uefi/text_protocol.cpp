/*
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "text_protocol.h"

#include <lib/cbuf.h>
#include <lib/io.h>
#include <platform/debug.h>
#include <stdio.h>

#include "events.h"

namespace {

int console_read_char(char *c) {
#if CONSOLE_HAS_INPUT_BUFFER
  return cbuf_read_char(&console_input_cbuf, c, false) == 1 ? 0 : -1;
#else
  return platform_dgetc(c, false);
#endif
}

}  // namespace

EfiStatus output_string(struct EfiSimpleTextOutputProtocol *self,
                        uint16_t *string) {
  char buffer[512];
  size_t i = 0;
  while (string[i]) {
    size_t j = 0;
    for (j = 0; j < sizeof(buffer) - 1 && string[i + j]; j++) {
      buffer[j] = string[i + j];
    }
    i += j;
    buffer[j] = 0;

    printf("%s", reinterpret_cast<const char *>(buffer));
  }
  return EFI_STATUS_SUCCESS;
}

EfiSimpleTextOutputProtocol get_text_output_protocol() {
  EfiSimpleTextOutputProtocol console_out;
  console_out.output_string = output_string;
  return console_out;
}

namespace {

cbuf_t key_buf;

void wait_for_key_notify(EfiEvent event, void *) {
  char c;
  bool got_key = false;
  while (console_read_char(&c) == 0) {
    cbuf_write_char(&key_buf, c, false);
    got_key = true;
  }
  if (got_key || cbuf_space_used(&key_buf) > 0) {
    signal_event(event);
  }
}

}  // namespace

EfiStatus read_key_stroke(struct EfiSimpleTextInputProtocol *self,
                          EfiInputKey *key) {
  char c;
  if (cbuf_read_char(&key_buf, &c, false) != 1 &&
      console_read_char(&c) != 0) {
    return EFI_STATUS_NOT_READY;
  }
  key->scan_code = 0;
  if (c == '\n') {
    c = '\r';
  }
  key->unicode_char = static_cast<unsigned char>(c);
  return EFI_STATUS_SUCCESS;
}

EfiStatus reset_input(struct EfiSimpleTextInputProtocol *self,
                      bool extended_verification) {
  char c;
  while (cbuf_read_char(&key_buf, &c, false) == 1) {
  }
  while (console_read_char(&c) == 0) {
  }
  return EFI_STATUS_SUCCESS;
}

static EfiSimpleTextInputProtocol console_in = {
    .reset = reset_input,
    .read_key_stroke = read_key_stroke,
    .wait_for_key = nullptr,
};

EfiSimpleTextInputProtocol *setup_text_input_protocol() {
  if (console_in.wait_for_key == nullptr) {
    cbuf_initialize(&key_buf, 128);
    create_event(EFI_EVENT_TYPE_NOTIFY_WAIT, 0, wait_for_key_notify, nullptr,
                 &console_in.wait_for_key);
  }
  return &console_in;
}

EfiSimpleTextInputProtocol *open_text_input_protocol() {
  return &console_in;
}
