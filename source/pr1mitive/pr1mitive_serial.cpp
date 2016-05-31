// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <cstdio>  // sprintf
#include <string.h> // strlen
#include "pr1mitive_serial.h"

using namespace pr1mitive::serial;

// Obtain a char from the charmap.
inline char get_char(int index) {
    static const char* charmap = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static size_t charmap_length = strlen(charmap);
    return charmap[index % charmap_length];
}

// Obtain a digit from the passed index.
inline char get_digit(int index) {
    return (index % 10) + '0';
}

// Generate a 4 character seed sequence from 11 Cinema 4D digits.
inline void generate_seed(const char* digits, char* dest) {
    for (int i=0; i < 4; i++) {
        int x = 0;
        for (int j=0; j < 3; j++) {
            int y = i * (j + 3);
            char s_num[3] = { digits[y % 11], digits[(y + i * 2 + j + 1) % 11], 0 };
            int num = 0;
            sscanf(s_num, "%d", &num);
            x += num * num;
        }
        dest[i] = get_char(x * (digits[4] + 1) + digits[1] - digits[6] + 1);
    }
}

// Generate a 3 character version-seed sequence from a version-number.
inline void generate_version(const int version, char* dest) {
    char string[20];
    int  string_length = sprintf(string, "%d", version);
    for (int i=0; i < 3; i++) {
        int x = 4567;
        for (int j=0; j < string_length; j++) {
            x += (i * (j + 1));
            x *= string[(x * i * 36 + j * 2 + 1) % string_length] - '0' + i + 1;
        }
        dest[i] = get_char(x);
    }
}

// Generate a 4 character hash from 7 characters.
inline void generate_hash(const char* input, char* dest) {
    for (int i=0; i < 4; i++) {
        int x = 9876;
        for (int j=0; j < 7; j++) {
            int y = (i * j * j + 16);
            x += input[y % 7] * y + i + 1;
        }
        dest[i] = get_char(x);
    }
}

// Generate a 4 character hash from 11 characters and 11 Cinema 4D digits.
inline void generate_sum(const char* digits, const char* input, char* dest) {
    for (int i=0; i < 4; i++) {
        int x = 143;
        for (int j=0; j < 11; j++) {
            int y = j * i * (digits[(j * 2) % 11] - '0' + 1) * 3;
            char s_num[3] = { (char) ((input[y % 11] % 10) + '0'), digits[(y + i * 2 + j + 1) % 11], 0 };
            int num = 0;
            sscanf(s_num, "%d", &num);
            x += (num << 3) * (num % ((i + 1) * ((j >> 2) + 1)));
        }
        dest[i] = get_char(x);
    }
}

// Generate 11 digits from 15 Multi-license digits. It will not touch the last 5 digits as they are
// important to the license-server.
void build_c4dlicense(const char* input, char* dest) {
    // Check if the multi-license part is correct.
    memcpy(dest, input, 11);

    int nums[3] = { 423, 943, 7432 };
    for (int i=6; i < 11; i++) {
        int j  = i + 6;

        // We can do this as it ultimately touches index 0 which is constant in a multi-license.
        char c1 = input[j];
        char c2 = input[(j + 1) % 18];

        nums[0] = c1 + c2 * 2;
        nums[1] = c1 * c2;
        nums[2] = c1 * 2 + c2 * 3;
    }

    dest[3] = get_digit(nums[0] * (dest[3] + 1));
    dest[4] = get_digit(nums[1] +  dest[4] + 6);
    dest[5] = get_digit(nums[2] * (dest[5] + 3));
}


C4DLicense::C4DLicense(const char* input) {
    size_t length;
    if (!input) {
        error = ERRORTYPE_NULLPOINTER;
        goto invalid;
    }

    length = strlen(input);
    if (length < 11) {
        error = ERRORTYPE_INVALIDLENGTH;
        goto invalid;
    }

    // Check for digits-only for the first 11 characters.
    for (int i=0; i < LICENSELENGTH_C4D; i++) {
        char c = input[i];
        if (c < '0' || c > '9') {
            error = ERRORTYPE_INVALIDPART_C4D;
            goto invalid;
        }
    }

    switch (length) {
        case LICENSELENGTH_C4D:
            type = LICENSETYPE_C4D;
            c4dlicense = new char[LICENSELENGTH_C4D + 1];
            memcpy(c4dlicense, input, length + 1);
            break;

        case LICENSELENGTH_ML:
            // Check for uppercase characters only for the last 6 digits.
            for (int i=LICENSELENGTH_C4D + 1; i < LICENSELENGTH_ML; i++) {
                char c = input[i];
                if (c < 'A' || c > 'Z') {
                    error = ERRORTYPE_INVALIDPART_ML;
                    goto invalid;
                }
            }
            type = LICENSETYPE_ML;
            c4dlicense = new char[LICENSELENGTH_C4D + 1]();
            build_c4dlicense(input, c4dlicense);
            break;

        default:
            error = ERRORTYPE_INVALIDLENGTH;
            goto invalid;
            break;
    }

    error = ERRORTYPE_NONE;
    return;

  invalid:
    type = LICENSETYPE_INVALID;
    c4dlicense = null;
    return;
}

C4DLicense::~C4DLicense() {
    if (c4dlicense) {
        delete [] c4dlicense;
        c4dlicense = null;
    }
}

SERIALRESULT C4DLicense::convert(char* dest) const {
    if (!c4dlicense || type == LICENSETYPE_INVALID) return SERIALRESULT_ERROR;
    generate_seed(c4dlicense, dest);
    generate_version(PR1MITIVE_VERSION_SERIALV, dest + 4);
    generate_hash(dest, dest + 7);
    generate_sum(c4dlicense, dest, dest + 11);
    return SERIALRESULT_OK;
}

SERIALRESULT C4DLicense::convert_pretty(char* dest) const {
    SERIALRESULT result = convert(dest);
    if (result != SERIALRESULT_OK) return result;

    char format[18];
    memcpy(format, dest, 4);
    format[4] = '-';
    memcpy(format + 5, dest + 4, 3);
    format[8] = '-';
    memcpy(format + 9, dest + 7, 4);
    format[13] = '-';
    memcpy(format + 14, dest + 11, 4);
    memcpy(dest, format, 18);
    return SERIALRESULT_OK;
}

SERIALRESULT C4DLicense::convert_unified(char* dest, int* length) const {
    int offset = 0;
    if (type == LICENSETYPE_ML) offset = LICENSELENGTH_C4D + 1;

    SERIALRESULT result = convert_pretty(dest + offset);
    if (result != SERIALRESULT_OK) return result;

    if (type == LICENSETYPE_ML) {
        memcpy(dest, c4dlicense, 11);
        dest[11] = '-';
    }
    if (length) *length = SERIALLENGTH_PRETTY + offset;
    return SERIALRESULT_OK;
}


