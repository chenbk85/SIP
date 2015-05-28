/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines a string table container, which is used for storing a blob
/// of string data to prevent small buffer allocations and string duplication.
///////////////////////////////////////////////////////////////////////////80*/

/*////////////////
//   Includes   //
////////////////*/

/*////////////////////
//   Preprocessor   //
////////////////////*/

/*/////////////////
//   Constants   //
/////////////////*/
/// @summary The array index used to store the number of items in a key bucket.
#define ST_BUCKET_SIZE      0U

/// @summary The array index used to store the capacity of a bucket.
#define ST_BUCKET_CAPACITY  1U

/// @summary The offset of the first key from the start of a bucket.
#define ST_KEY_OFFSET       2U

/// @summary The default number of strings per-bucket. Used for pre-allocation.
#define ST_ITEMS_PER_BUCKET 128U

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Function signature for a string comparison function. 
/// @param Pointer to a NULL-terminated UTF-8 string.
/// @param Pointer to a NULL-terminated UTF-8 string.
/// @return true if the two strings are equal; false otherwise.
typedef bool     (*str_cmp_fn )(char const *, char const *);

/// @summary Function signature for a string hash function.
/// @param Pointer to a NULL-terminated UTF-8 string.
/// @param On return, points to one past the zero byte of the input string.
/// @return A 32-bit unsigned integer hash value.
typedef uint32_t (*str_hash_fn)(char const *, char const *&);

/// @summary Defines the data maintained by a string table. The string table 
/// stores interned string data, terminated by a zero byte, and uses an array
/// hash table to accelerate string lookup.
struct string_table_t
{
    size_t        StringCount;    /// The number of strings interned in the memory buffer.
    char         *NextBlock;      /// Pointer to the next available byte (write cursor).
    char         *MemoryStart;    /// Pointer to the start of the allocated memory buffer.
    char         *MemoryEnd;      /// Pointer to one byte past the end of the allocated memory buffer.
    size_t        BytesUsed;      /// The number of string buffer bytes currently used.
    size_t        BytesTotal;     /// The total number of string buffer bytes allocated.
    size_t        BucketCount;    /// The number of hash buckets allocated.
    size_t        BucketMask;     /// The bitmask used to map a hash value into the bucket array.
    str_cmp_fn    StringCompare;  /// The string comparison function, testing two strings for equality.
    str_hash_fn   StringHash;     /// The hash function, mapping zero-terminated string to 32-bit hash value.
    uint32_t    **KBuckets;       /// The array of variable-length key buckets (32-bit hashes of string data).
    size_t      **VBuckets;       /// The array of variable-length value buckets (byte offsets into the memory buffer).
};

/*///////////////
//   Globals   //
///////////////*/

/*///////////////////////
//   Local Functions   //
///////////////////////*/
/// @summary Compare two strings in a case-sensitive manner.
/// @param a A NULL-terminated UTF-8 string.
/// @param b A NULL-terminated UTF-8 string.
/// @return true if the strings are equal.
internal_function bool default_string_compare_cs(char const *a, char const *b)
{
    return strcmp(a, b) == 0;
}

/// @summary Compare two strings, ignoring case differences.
/// @param a A NULL-terminated UTF-8 string.
/// @param b A NULL-terminated UTF-8 string.
/// @return true if the strings are equal.
internal_function bool default_string_compare_ci(char const *a, char const *b)
{
    return _stricmp(a, b) == 0;
}

/// @summary Calculates a 32-bit hash value for a string. Character case matters.
/// @param str A NULL-terminated UTF-8 string.
/// @param end On return, points to one past the zero byte of the input string.
/// @return A 32-bit hash value for the string.
internal_function uint32_t default_string_hash_cs(char const *str, char const *&end)
{
    uint32_t hash = 0;
    if (str != NULL)
    {
        uint32_t    ch   = 0;
        char const *iter = str;
        while ((ch = *iter++) != 0)
        {
            hash = _lrotl(hash, 7) + ch;
        }
        end = iter;
    }
    else end = str + 1;
    return hash;
}

/// @summary Calculates a 32-bit hash value for a string. Character case is ignored.
/// @param str A NULL-terminated UTF-8 string.
/// @param end On return, points to the one past the zero byte of the input string.
/// @return A 32-bit hash value for the string.
internal_function uint32_t default_string_hash_ci(char const *str, char const *&end)
{
    uint32_t hash = 0;
    if (str != NULL)
    {
        uint32_t    ch   = 0;
        char const *iter = str;
        while ((ch = *iter++) != 0)
        {
            hash = _lrotl(hash, 7) + ((ch >= 'A' && ch <= 'Z') ? ((ch-'A')+'a') : ch);
        }
        end = iter;
    }
    else end = str + 1;
    return hash;
}

/// @summary Calculates a 32-bit hash value for a path string. Forward and backslashes are treated as equivalent, and character case is ignored.
/// @param str A NULL-terminated UTF-8 path string.
/// @param end On return, points to one past the zero byte of the input string.
/// @return The hash of the specified string.
internal_function uint32_t string_hash_path(char const *str, char const *&end)
{
    uint32_t hash = 0;
    if (str != NULL)
    {
        uint32_t    ch   = 0;
        char const *iter = str;
        while ((ch = *iter++) != 0)
        {
            hash = _lrotl(hash, 7) + ((ch >= 'A' && ch <= 'Z') ? ((ch-'A')+'a') : ((ch == '\\') ? '/' : ch));
        }
        end = iter;
    }
    else end = str + 1;
    return hash;
}

/*////////////////////////
//   Public Functions   //
////////////////////////*/
/// @summary Initializes and reserves memory for a string table.
/// @param table The string table to initialize.
/// @param capacity The number of strings expected to be interned within the table.
/// @param average_length The average length of an interned string, in bytes.
/// @return true if all storage was successfully pre-allocated.
public_function bool string_table_create(string_table_t *table, size_t capacity, size_t average_length=0)
{
    if (capacity < ST_ITEMS_PER_BUCKET)
    {   // use the minimum capacity.
        capacity = ST_ITEMS_PER_BUCKET;
    }
    // initialize and reserve memory for string data storage.
    size_t blob_bytes      = capacity * average_length;
    table->StringCount     = 0;
    table->NextBlock       = NULL;
    table->MemoryStart     = NULL;
    table->MemoryEnd       = NULL;
    table->BytesUsed       = 0;
    table->BytesTotal      = 0;
    if (blob_bytes > 0)
    {   // reserve memory for string storage.
        if ((table->MemoryStart = (char*) malloc(blob_bytes)) == NULL)
        {   // unable to allocate the required memory.
            return false;
        }
        table->NextBlock   = table->MemoryStart;
        table->MemoryEnd   = table->MemoryStart + blob_bytes;
        table->BytesTotal  = blob_bytes;
    }

    // initialize and reserve memory for hash table storage.
    table->BucketCount     = next_pow2(capacity / ST_ITEMS_PER_BUCKET);
    table->BucketMask      = table->BucketCount - 1;
    table->StringCompare   = default_string_compare_ci;
    table->StringHash      = default_string_hash_ci;
    table->KBuckets = (uint32_t**) malloc(table->BucketCount * sizeof(uint32_t*));
    table->VBuckets = (size_t  **) malloc(table->BucketCount * sizeof(size_t  *));
    for (size_t   i = 0; i < table->BucketCount; ++i)
    {
        table->KBuckets[i] = (uint32_t*) malloc((ST_ITEMS_PER_BUCKET+2) * sizeof(uint32_t));
        table->VBuckets[i] = (size_t  *) malloc( ST_ITEMS_PER_BUCKET    * sizeof(size_t  ));
        table->KBuckets[i][ST_BUCKET_SIZE]     = 0;
        table->KBuckets[i][ST_BUCKET_CAPACITY] = ST_ITEMS_PER_BUCKET;
    }
    return true;
}

/// @summary Frees all resources associated with a string table.
/// @param table The string table to delete.
public_function void string_table_delete(string_table_t *table)
{
    for (size_t i = 0; i < table->BucketCount;  ++i)
    {
        free(table->VBuckets[i]); table->VBuckets[i] = NULL;
        free(table->KBuckets[i]); table->KBuckets[i] = NULL;
    }
    free(table->VBuckets);    table->VBuckets    = NULL;
    free(table->KBuckets);    table->KBuckets    = NULL;
    free(table->MemoryStart); table->MemoryStart = NULL;
    table->NextBlock  = NULL;
    table->MemoryEnd  = NULL;
    table->BucketCount= 0;
    table->BucketMask = 0;
    table->StringCount= 0;
    table->BytesUsed  = 0;
    table->BytesTotal = 0;
}

/// @summary Intern a string. If the string exists, return the interned data.
/// @param table The string table to update.
/// @param str The NULL-terminated UTF-8 string to intern.
/// @param byte_offset On return, this value is set to the unique byte offset of the interned string data.
/// @return A pointer to the interned string data. This pointer is valid until the next put operation.
public_function char* string_table_put(string_table_t *table, char const *str, size_t &byte_offset)
{   // attempt to locate an existing string match within the table.
    char const *ep   = NULL;
    uint32_t  hash   = table->StringHash(str,ep);
    size_t    nbytes = size_t(ep - str);
    size_t    bucket = hash & table->BucketMask;
    uint32_t *klist  = table->KBuckets[bucket];
    size_t   *vlist  = table->VBuckets[bucket];
    for (size_t ki   = 0, kc = klist[ST_BUCKET_SIZE]; ki < kc; ++ki)
    {
        if (klist[ki + ST_KEY_OFFSET] == hash)
        {   // possible match - confirm by performing a string compare.
            if (table->StringCompare(str, table->MemoryStart + vlist[ki]))
            {   // definite match - no need to re-intern the string.
                byte_offset = vlist[ki];
                return table->MemoryStart + vlist[ki];
            }
        }
    }

    // no existing match, so we will intern the string.
    byte_offset   = table->NextBlock - table->MemoryStart;
    size_t   bytes_needed = table->BytesUsed + nbytes;
    if (table->BytesTotal < bytes_needed)
    {   // need to grow the string data storage buffer.
        size_t new_size   = bytes_needed+(64 * 1024);
        char  *new_buffer =(char*)realloc(table->MemoryStart, new_size);
        if (new_buffer != NULL)
        {   // the requested buffer was successfully reallocated.
            table->BytesTotal  = new_size;
            table->MemoryStart = new_buffer;
            table->MemoryEnd   = new_buffer + new_size;
            table->NextBlock   = new_buffer + byte_offset;
        }
        else
        {   // unable to allocate the required memory, so fail.
            byte_offset = 0;
            return NULL;
        }
    }

    // copy the string data into the internal buffer.
    memcpy(table->NextBlock, str, nbytes);
    
    // if necessary, grow the storage for the hash table bucket.
    if (klist[ST_BUCKET_SIZE] == klist[ST_BUCKET_CAPACITY])
    {   // need to grow the hash bucket.
        size_t    new_size  = klist[ST_BUCKET_CAPACITY] + ST_ITEMS_PER_BUCKET;
        uint32_t *new_keys  =(uint32_t*) realloc(table->KBuckets[bucket], new_size * sizeof(uint32_t));
        size_t   *new_vals  =(size_t  *) realloc(table->VBuckets[bucket], new_size * sizeof(size_t  ));
        if (new_keys != NULL) table->KBuckets[bucket] = new_keys;
        if (new_vals != NULL) table->VBuckets[bucket] = new_vals;
        if (new_keys != NULL && new_vals != NULL)
        {   // memory allocation was successful, store the updated capacity.
            new_keys[ST_BUCKET_CAPACITY]  = (uint32_t)  new_size;
            klist  = new_keys;
            vlist  = new_vals;
        }
        else
        {   // unable to allocate the required memory, so fail.
            byte_offset = 0;
            return NULL;
        }
    }

    // update the hash table with key = hash and value = byte_offset.
    size_t table_index = klist[ST_BUCKET_SIZE]++;
    klist [table_index + ST_KEY_OFFSET] = hash;
    vlist [table_index]= byte_offset;

    // all operations completed successfully.
    table->NextBlock  += nbytes;
    table->BytesUsed  += nbytes;
    table->StringCount++;
    return table->MemoryStart + byte_offset;
}

/// @summary Translate a string ID (table byte offset) into the string pointer.
/// @param table The string table to query.
/// @param byte_offset The byte offset of the string within the table.
/// @return A pointer to the interned string data. The returned pointer is valid only until the next put operation.
public_function char const* string_table_get(string_table_t *table, size_t byte_offset)
{
    assert(byte_offset < table->BytesUsed);
    return table->MemoryStart + byte_offset;
}

/// @summary Reset the string table to empty without freeing any memory.
/// @param table The string table to reset.
public_function void string_table_clear(string_table_t *table)
{   // clear out all of the hash buckets.
    for (size_t i = 0, n = table->BucketCount; i < n; ++i)
    {
        table->KBuckets[i][ST_BUCKET_SIZE] = 0;
    }
    // reset the stack allocator for the string buffer.
    table->NextBlock   = table->MemoryStart;
    table->BytesUsed   = 0;
    table->StringCount = 0;
}
