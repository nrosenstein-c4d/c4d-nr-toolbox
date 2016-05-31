#ifndef Gvcsv_H
#define Gvcsv_H

    enum {
        Gvcsv = 1030755,
        CSVNODE_FILENAME = 19000,   // Filename [in]
        CSVNODE_HEADER,             // Bool   [in]
        CSVNODE_DELIMITER,          // LONG   [in] 0 <= x <= 255
        CSVNODE_ROWINDEX,           // LONG   [in]

        // Non-description IDs
        CSVNODE_LOADED,             // Bool   [out]
        CSVNODE_COLCOUNT_TOTAL,     // LONG   [out]
        CSVNODE_COLCOUNT,           // LONG   [out]
        CSVNODE_ROWCOUNT,           // LONG   [out]
        CSVNODE_DYNPORT_START,      // String [out, dynamic]

        CSVNODE_DELIMITER_COMMA = 44,
        CSVNODE_DELIMITER_SEMICOLON = 59,
        CSVNODE_DELIMITER_TAB = 9,

        // Other attributes
        CSVNODE_FORCEOUTPORTS = 20000,  // BOOL
        CSVNODE_FORCEOUTPORTS_COUNT,    // LONG    
    };

#endif /* Gvcsv */

