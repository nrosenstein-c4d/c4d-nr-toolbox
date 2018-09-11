/**
 * Copyright (C) 2013-2015 Niklas Rosenstein
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 */

#ifndef NR_LIB_CSV_H
#define NR_LIB_CSV_H

    #include <c4d_apibridge.h>

    #if API_VERSION < 15000
        /* From R15.020 maxon/utilities/apibasemath.h */
        /// Calculates the minimum of two values and return it.
        template <typename X> inline X Min(X a, X b) { if (a<b) return a; return b; }

        /// Calculates the maximum of two values and return it.
        template <typename X> inline X Max(X a, X b) { if (a<b) return b; return a; }
    #endif

    /**
     * Extension of the maxon::BaseArray class implementing the retrieving
     * of default values.
     */
    template <typename T>
    class DefaultBaseArray : public maxon::BaseArray<T> {

        typedef maxon::BaseArray<T> super;

    public:

        /**
         * Retrieve a value by index. If the index is out of range, the default
         * value is returned.
         */
        const T& GetIndex(Int32 index, const T& default_) const {
            if (index < 0 && index >= super::_cnt) return default_;
            else return super::operator [] (index);
        }

        /**
         * Retrieve a value by index. If the index is out of range, the default
         * value is returned.
         */
        T& GetIndex(Int32 index, T& default_) {
            if (index < 0 && index >= super::_cnt) return default_;
            else return super::operator [] (index);
        }

    };

    /**
     * A row from a CSV file is nothing but an array of strings. :-)
     */
    typedef DefaultBaseArray<String> CSVRow;

    enum CSVError {
        CSVError_None,          // No error, everything worked out fine.
        CSVError_Permission,    // Not enough permission to read from the file.
        CSVError_Memory,        // Memory error, eg. allocation failed
        CSVError_Unknown,       // Unkown error
        CSVError_FileError,     // Use CSVReader::GetFileError()
        CSVError_InvalidFile,   // It was determined that the file can't be a CSV file, not used atm.
    };

    /**
     * A CSV reader implementation. Initialize once only.
     */
    class CSVReader {

    public:

        CSVReader(Char delimiter=',', Bool stripWhitespace=true);
        virtual ~CSVReader();

        /**
         * Open a file with the specified filename. Returns true if the file
         * could be opened, false if not. Do not perform any reading operations
         * if opening a file failed.
         */
        Bool Open(Filename fl) {
            if (m_isOpened) {
                return false; // already a file opened.
            }
            m_isOpened = m_file->Open(fl, FILEOPEN_READ);
            m_atEnd = false;
            m_line = 0;

            switch (GetFileError()) {
                case FILEERROR_NONE:
                    break;
                case FILEERROR_OUTOFMEMORY:
                    m_error = CSVError_Memory;
                    break;
                case FILEERROR_OPEN:
                    m_error = CSVError_Permission;
                    break;
                default:
                    m_error = CSVError_Unknown;
                    break;
            }

            m_isOpened = m_isOpened && m_error == CSVError_None;
            return m_isOpened;
        }

        /**
         * Close the opened CSV file. It is safe to call this method even if
         * no file was opened before. Automatically called when the reader
         * is destroyed.
         */
        Bool Close() {
            if (!m_isOpened) {
                return true;
            }
            m_isOpened = false;
            m_atEnd = true;
            return m_file->Close();
        }

        /**
         * Return true if the reader has opened a file.
         */
        Bool IsOpened() {
            return m_isOpened;
        }

        /**
         * Read a line from the opened file and append the cell values to the
         * `destRow`. Returns true on success, false if a fatal error occured.
         * Also returns false if the end of the file has been reached, but
         * better use `AtEnd()` instead.
         */
        Bool GetRow(CSVRow& destRow);

        /**
         * Returns true if the end of the input file has been reached.
         */
        Bool AtEnd() const {
            return m_atEnd;
        }

        /**
         * Return the current line-number in the input file.
         */
        Int32 GetLineNumber() const {
            return m_line;
        }

        /**
         * Retrieve the latest error. If an error occured, `GetRow()` will not
         * read from the file any further until `FlushError()` is called.
         */
        CSVError GetError() const { return m_error; }

        /**
         * Get the FILEERROR value from the internal BaseFile.
         */
        FILEERROR GetFileError() const { return m_file->GetError(); }

    private:

        String CharArrayToString(const maxon::BaseArray<Char>& arr);

        Bool m_isOpened;
        Bool m_atEnd;
        Int32 m_line;
        AutoAlloc<BaseFile> m_file;
        Char m_delimiter;
        Bool m_stripWhitespace;
        STRINGENCODING m_encoding;
        CSVError m_error;

    };

    /**
     * This class manages the reading of a CSV Table. Everytime the CSV data
     * is request, the `Init()` method can be called before to ensure the
     * correct data is loaded.
     *
     * However, this is just a base-class for managing this safe reading
     * mechanism. Subclasses should implement the storing and retrieving
     * of the actual values.
     */
    class BaseCSVTable {

    public:

        /**
         * Initialize the BaseCSVTable.
         */
        BaseCSVTable(Char delimiter=',', Bool hasHeader=false)
        : m_delimiter(delimiter), m_hasHeader(hasHeader), m_loaded(false),
          m_error(CSVError_None), m_fileError(FILEERROR_NONE) { }

        /**
         * Virtual destructor.
         */
        virtual ~BaseCSVTable() { }

        /**
         * Initialize the BaseCSVTable object. This method checks if the data
         * from the passed filename requires to be reloaded or not. This method
         * calls `CheckReload()` if *forceUpdate* is passed false. If *didReload*
         * is passed a valid pointer, it is assigned true in case the reloading
         * of data was performed. This will also be perfomed if an error occured,
         * so basically *didReload* will be true when *forceUpdate* is passed or
         * `CheckReload()` returned true.
         *
         * If this method returns false, a fatal error must have occured (such
         * as insufficient reading permissions, a memory error, etc.). Use
         * `GetLastError()` to retrieve error-information.
         *
         * Note: It is not an error if the CSV File does not exist!
         * `LoadDataStart()` and `LoadDataEnd()` WILL BE CALLED if the file
         * does not exist.
         */
        Bool Init(const Filename& filename, Bool forceUpdate=false, Bool* didReload=nullptr);

        /**
         * Retrieves the type of the error which occured at the last call
         * to `Init()`.
         */
        CSVError GetLastError() const { return m_error; }

        /**
         * Return the last FILEERROR information.
         */
        FILEERROR GetFileError() const { return m_fileError; }

        /**
         * Specify if the CSV Table has a header. If this was set to
         * true, the first row is read in as the table header and can
         * be retrieved via `GetTableHeader()`.
         */
        void SetHasHeader(Bool hasHeader) { m_hasHeader = hasHeader; }

        /**
         * Returns true if it was specified that the CSV Table has a
         * header. This can be set via `SetHasHeader()`.
         */
        Bool GetHasHeader() const { return m_hasHeader; }

        /**
         * Set the delimiter to use in the CSV Reader.
         */
        void SetDelimiter(Char delimiter) { m_delimiter = delimiter; }

        /**
         * Return the delimiter that will be used in the CSV Reader.
         */
        Char GetDelimiter() const { return m_delimiter; }

        /**
         * Returns the header of the CSV Table. This will return a valid
         * pointer if it was specified that the CSV Table does have a
         * header (via `SetHasHeader()` or the constructor).
         */
        const CSVRow& GetHeader() const { return m_header; }

        /**
         * Returns true if the CSV File was successfully loaded the last time
         * `Init()` was called.
         */
        Bool Loaded() const { return m_loaded; }

    protected:

        /**
         * Returns true if the CSV Data requires to be reloaded based on the
         * passed filename. The default implementation checks if the filename
         * differs, and if it does not, if the file has changed and returns
         * true respectively to trigger a reload of the data.
         */
        virtual Bool CheckReload(const Filename& filename);

        /**
         * Called to flush the stored data.
         */
        virtual void FlushData() = 0;

        /**
         * Called before data is read in. Return CSVError_None if everything
         * worked out fine. If any other value is returned, the reading in of
         * data is halted. Even if this method returns an error-code,
         * `LoadDataEnd()` is still called!
         */
        virtual CSVError LoadDataStart(const Filename& filename) = 0;

        /**
         * Called after the data was read in. Passed is a boolean value that
         * is true when the reading was successful, otherwise it is false.
         */
        virtual void LoadDataEnd(CSVError error) = 0;

        /**
         * Called when the header was loaded. The header array is stored by
         * the BaseCSVTable, but this method allows to process it and create
         * new data out of it. This method is not called if it was not
         * specified that a header should be read.
         */
        virtual CSVError ProcessHeader(const CSVRow& header) { return CSVError_None; }

        /**
         * Process the CSV Row passed to this method and store it. Return
         * an error-code only when a fatal error occured. Reading the CSV File
         * will then be halted. Return CSVError_None if everything worked out
         * fine. Make sure to store a copy of the row.
         */
        virtual CSVError StoreRow(const CSVRow& row) = 0;

        /**
         * Return the number of rows stored.
         */
        virtual Int32 GetRowCount() const = 0;

        /**
         * Return the number of columns. This is the maximum number of
         * cells in all rows.
         */
        virtual Int32 GetColumnCount() const = 0;

    private:

        Char m_delimiter;
        Bool m_hasHeader;

        Bool m_loaded;
        CSVError m_error;
        FILEERROR m_fileError;
        CSVRow m_header;
        Filename m_filename;
        LocalFileTime m_fileTime;

    };

    /**
     * This template subclass of the BaseCSVTable class converts all cells
     * to values of a given type by using the a specified callback. The
     * `Converter` argument must be callable and implement the following
     * signature: `T (*)(const String& cell)`.
     */
    template <typename T, T (*Converter)(const String&)>
    class TypedCSVTable : public BaseCSVTable {

        typedef BaseCSVTable super;

    public:

        typedef maxon::BaseArray<T> Array;

        TypedCSVTable(Char delimiter=',', Bool hasHeader=false)
        : super(delimiter, hasHeader), m_columnCount(0) { }

        virtual ~TypedCSVTable() { }

        /**
         * Obtain a stored row from the table. Does not perfom in-bound checks!
         */
        virtual const Array& GetRow(Int32 index) const { return m_rows[index]; }

        /**
         * Callback method when a converted row was stored.
         */
        virtual void RowStored(const Array& row) { }

        //| BaseCSVTable Overrides

        virtual void FlushData() {
            m_rows.Reset();
            m_columnCount = 0;
        }

        virtual CSVError LoadDataStart(const Filename& filename) {
            return CSVError_None;
        }

        virtual void LoadDataEnd(CSVError error) {
            return;
        }

        virtual CSVError StoreRow(const CSVRow& row) {
            Int32 count = row.GetCount();
            // Defined in c4d_misc/utilities/apibasemath.h
            m_columnCount = Max<Int32>(m_columnCount, count);

            Array newRow;
            iferr (newRow.Resize(count))
                return CSVError_Memory;
            for (Int32 i=0; i < count; i++) {
                newRow[i] = Converter(row[i]);
            }
            iferr (Array& rowRef = m_rows.Append(std::move(newRow)))
                return CSVError_Memory;
            RowStored(rowRef);
            return CSVError_None;
        }

        virtual Int32 GetRowCount() const { return m_rows.GetCount(); }

        virtual Int32 GetColumnCount() const { return m_columnCount; }

    private:

        Int32 m_columnCount;
        maxon::BaseArray<Array> m_rows;

    };

    /**
     * Strips all whitespace characters from the beginning and end of astring.
     */
    String StringStripWhitespace(const String& ref);

    /**
     * Convert all elements in a CSVRow into another type based on a converter
     * function.
     */
    template <typename DestType> void ConvertCSVRow(
            const CSVRow& row,
            maxon::BaseArray<DestType>& dest,
            DestType (*converter)(const String&)) {
        // Construct a new array based on the template type.
        dest.Resize(row.GetCount());

        // Iterate over each element in the array.
        for (Int32 i=0; i < row.GetCount(); i++) {
            DestType v = converter(row[i]);
            dest[i] = v;
        }
    }

    /**
     * Convert a String to a Int32. Returns zero on failure.
     */
    Int32 StringToLong(const String& str);

    /**
     * Convert a string to a decimal number. Returns zero on failure.
     */
    Float StringToReal(const String& str);

    /**
     * Returns a copy of the string.
     */
    inline String StringToString(const String& str) {
        return str;
    }

    typedef TypedCSVTable<String, StringToString> StringCSVTable;
    typedef TypedCSVTable<Int32, StringToLong> Int32CSVTable;
    typedef TypedCSVTable<Float, StringToReal> FloatCSVTable;

    /**
     * This float CSV Table subclass implements retrieving minimum and
     * maximum values, as well as creating a `BaseContainer` that contains
     * the header-names for the columns.
     */
    class MMFloatCSVTable : public FloatCSVTable {

        typedef FloatCSVTable super;

    public:

        MMFloatCSVTable(Char delimiter=',', Bool hasHeader=false)
        : super(delimiter, hasHeader) { }

        /**
         * Return the minimum values for all columns. Only valid if the table
         * successfully initialized.
         */
        const super::Array& GetMinValues() const { return m_minv; }

        /**
         * Return the maximum values for all columns. Only valid if the table
         * successfully initialized.
         */
        const super::Array& GetMaxValues() const { return m_maxv; }

        /**
         * Returns a container, formatted, ready to be injected into a cycle
         * description parameter.
         */
        const BaseContainer& GetHeaderContainer() const { return m_headBc; }

        //| TypedCSVTable<Float> Overrides

        virtual void RowStored(const Array& row) {
            Int32 count = row.GetCount();
            (void) m_minv.Resize(count);
            (void) m_maxv.Resize(count);
            (void) m_mmSet.Resize(count);

            for (Int32 i=0; i < count; i++) {
                Bool& isSet = m_mmSet[i];
                if (!isSet || row[i] < m_minv[i]) {
                    m_minv[i] = row[i];
                }
                if (!isSet || row[i] > m_maxv[i]) {
                    m_maxv[i] = row[i];
                }
                isSet = true;
            }
        }

        //| BaseCSVTable Overrides

        virtual void FlushData() {
            m_minv.Reset();
            m_maxv.Reset();
            m_headBc.FlushAll();
            super::FlushData();

            GePrint("Reloading CSV File.."_s);
        }

        virtual void LoadDataEnd(CSVError error) {
            m_mmSet.Reset();
            super::LoadDataEnd(error);
            if (error != CSVError_None) return;

            m_headBc.SetString(-1, "-"_s);
            const CSVRow& header = GetHeader();
            for (Int32 i=0; i < GetColumnCount(); i++) {
                if (i < header.GetCount()) {
                    m_headBc.SetString(i, header[i]);
                }
                else {
                    m_headBc.SetString(i, String::IntToString(i));
                }
            }
        }

    private:

        // Used temporarily in StoreRow(), reset in LoadDataEnd();
        maxon::BaseArray<Bool> m_mmSet;

        super::Array m_minv;
        super::Array m_maxv;
        BaseContainer m_headBc;

    };

#endif /* NR_LIB_CSV_H */
