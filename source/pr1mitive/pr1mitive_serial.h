// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#define PR1MITIVE_EXTERNAL_INCLUDE
#include <pr1mitive/version.h>
#include <pr1mitive/defines.h>

#ifndef PR1MITIVE_SERIAL_H
#define PR1MITIVE_SERIAL_H

namespace pr1mitive {
namespace serial {

    static const char SERIALLENGTH = 15;
    static const char SERIALLENGTH_PRETTY = 18;
    static const char SERIALLENGTH_UNIFIED = SERIALLENGTH_PRETTY + 1 + 11;

    static const char LICENSELENGTH_C4D = 11;
    static const char LICENSELENGTH_ML = 18;

    typedef enum SERIALRESULT {
        SERIALRESULT_OK,
        SERIALRESULT_ERROR
    } SERIALRESULT;

    typedef enum LICENSETYPE {
        LICENSETYPE_INVALID = 0,
        LICENSETYPE_C4D,
        LICENSETYPE_ML
    } LICENSETYPE;

    typedef enum ERRORTYPE {
        ERRORTYPE_NONE = 0,
        ERRORTYPE_NULLPOINTER,
        ERRORTYPE_INVALIDLENGTH,
        ERRORTYPE_INVALIDPART_C4D,
        ERRORTYPE_INVALIDPART_ML
    } ERRORTYPE;

    // This class accepts a Cinema 4D License on construction and has methods to
    // convert it to a Pr1mitive Serial sequence.
    class C4DLicense {

      private:

        C4DLicense(const C4DLicense& other) {
            type = other.type;
            c4dlicense = other.c4dlicense;
        };

        C4DLicense& operator = (const C4DLicense& other) {
            type = other.type;
            c4dlicense = other.c4dlicense;
            return *this;
        };

      public:

        ERRORTYPE error;
        LICENSETYPE type;
        char* c4dlicense;

        // Pass the Cinema 4D License on construction. The input-string is copied internall.y
        // Check the `type` attribute after construction if the passed license was valid.
        C4DLicense(const char* c4dlicense);

        // Frees allocated memory.
        ~C4DLicense();

        // Convert the C4D License to a concatenated Pr1mitive license. The passed buffer must
        // be at least SERIALLENGTH bytes.
        SERIALRESULT convert(char* dest) const;

        // Convert the C4D License to a prettified (more human-readable) Pr1mitive license. The
        // passed buffer must be at least SERIALLENGTH_PRETTY bytes.
        SERIALRESULT convert_pretty(char* dest) const;

        // Convert the C4D License to a prettified Pr1mitive license with respect to multi-license
        // types. The passed buffer must be at least SERIALLENGTH_UNIFIED bytes. The string will
        // NOT be nullptr terminated. This is left to the user. The length of the string inserted
        // in `dest` is saved in `length` (can be nullptr).
        SERIALRESULT convert_unified(char* dest, int* length=nullptr) const;

    };

} // end namespace serial
} // end namespace pr1mitive

#endif // PR1MITIVE_SERIAL_H

