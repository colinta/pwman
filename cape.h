/*
    _____  _____   _____   _____
   |      |_____| |_____| |_____
   |_____ |     | |       |_____  version 2.0

Cape Copyright (c) 2012-2017, Giovanni Blu Mitolo All rights reserved.

Modified heavily by Colin Gray (see https://github.com/gioblu/Cape for the
original source code).

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#pragma once

typedef struct {
    char salt; // Salt used for encryption, can be exchanged if encrypted
    char *key;          //
    uint16_t length;    // Keep these private and safe
    char reduced_key;   //
} cape_t;

void cape_init(cape_t *cape, char *key, uint16_t length, uint8_t s);
void cape_crypt(cape_t *cape, char *source, char *destination, uint16_t length);
bool process_record_cape(char keycode, cape_t *cape, char *cape_password, uint16_t *cape_password_length, uint16_t salt);

/// CAPE

/* Compute a 1 byte version of the encryption key */
uint16_t cape_compute_reduced_key(char *key, uint16_t length) {
    uint16_t reduced_key = 0;
    // Reduced key computation
    for(uint16_t i = 0; i < length; i++) {
        reduced_key ^= (key[i] << (i % 8));
    }
    return reduced_key;
}

void cape_init(cape_t *cape, char *key, uint16_t length, uint8_t salt) {
    cape->salt = salt;
    cape->key = key;
    cape->length = length;
    cape->reduced_key = cape_compute_reduced_key(key, length);
}

/* Symmetric chiper using private key, reduced key and optionally salt:
   (max 65535 characters) */
void cape_crypt(cape_t *cape,
    char *source,
    char *destination,
    uint16_t length)
{
    for(uint16_t i = 0; i < length; i++) {
        destination[i] = (
            (cape->reduced_key ^ source[i] ^ cape->salt ^ i) ^
            cape->key[(cape->reduced_key ^ cape->salt ^ i) % cape->length]
        );
    }
    destination[length] = 0;
}
