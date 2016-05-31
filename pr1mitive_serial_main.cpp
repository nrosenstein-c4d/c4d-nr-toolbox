// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pr1mitive/pr1mitive_serial.h>

#define LENGTH_DIGITS       11
#define LENGTH_MULTILICENSE 18

using namespace pr1mitive::serial;

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: serial [c4d-digits]\n");
        return EINVAL;
    }

    // All serials will be stored here.
    char** serials = new char*[argc - 1];

    // Iterate over each input-serial and generate a new serial.
    for (int i=1; i < argc; i++) {
        char* digits = argv[i];
        C4DLicense license(digits);
        if (license.type == LICENSETYPE_INVALID) {
            printf("License %d (%s) is invalid. Error-code is %d\n", i, digits, license.error);
            return EINVAL;
        }

        char* serial = new char[SERIALLENGTH_UNIFIED + 1]();
        SERIALRESULT result = license.convert_unified(serial);
        if (result != SERIALRESULT_OK) {
            printf("License %d (%s) could not be converted.\n", i, digits);
            return EINVAL;
        }

        serials[i - 1] = serial;
    }

    // Output all serials.
    for (int i=0; i < (argc - 1); i++) {
        printf("%s\n", serials[i]);
    }

    return 0;
}


