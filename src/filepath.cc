/*/////////////////////////////////////////////////////////////////////////////
/// @summary Path string parsing and manipulation, directory enumeration.
///////////////////////////////////////////////////////////////////////////80*/

/*////////////////////
//   Preprocessor   //
////////////////////*/

/*////////////////
//   Includes   //
////////////////*/

/*/////////////////
//   Constants   //
/////////////////*/
/// @summary Path storage for a file list grows in 1024 item increments after
/// it hits 1024 items in size; prior to that point, it doubles in size.
static size_t const FILE_LIST_PATH_GROW_LIMIT =  1024;

/// @summary Blob storage for a file list grows in 64KB chunks after it hits
/// 64KB in size; prior to that point, it doubles in size.
static size_t const FILE_LIST_BLOB_GROW_LIMIT = (64 * 1024 * 1024);

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Represents a growable list of UTF-8 file paths. Designed to support
/// load-in-place, so everything must be explicitly sized, and should generally
/// store byte offsets instead of pointers within data buffers.
struct file_list_t
{
    uint32_t   PathCapacity; /// The current capacity, in paths.
    uint32_t   PathCount;    /// The number of paths items currently in the list.
    uint32_t   BlobCapacity; /// The current capacity of PathData, in bytes.
    uint32_t   BlobCount;    /// The number of bytes used in PathData.
    uint32_t   MaxPathBytes; /// The maximum length of any path in the list, in bytes.
    uint32_t   TotalBytes;   /// The total number of bytes allocated.
    uint32_t  *HashList;     /// Hash values calculated for the paths in the list.
    uint32_t  *SizeList;     /// Length values (not including the zero byte) for each path.
    uint32_t  *PathOffset;   /// Offsets, in bytes, into PathData to the start of each path.
    char      *PathData;     /// Raw character data. PathList points into this blob.
};

/*///////////////
//   Globals   //
///////////////*/

/*///////////////////////
//   Local Functions   //
///////////////////////*/
/// @summary Calculates the number of items by which to grow a dynamic list.
/// @param value The current capacity.
/// @param limit The number of items beyond which the capacity stops doubling.
/// @param min_value The minimum acceptable capacity.
internal_function inline size_t file_list_grow_size(size_t value, size_t limit, size_t min_value)
{
    size_t new_value = 0;

    if (value >= limit)
        new_value = value + limit;
    else
        new_value = value * 2;

    return new_value >= min_value ? new_value : min_value;
}

/// @summary Retrieves the next UTF-8 codepoint from a string.
/// @param str Pointer to the start of the codepoint.
/// @param cp On return, this value stores the current codepoint.
/// @return A pointer to the start of the next codepoint.
internal_function inline char const* next_utf8_codepoint(char const *str, uint32_t &cp)
{
    if ((str[0] & 0x80) == 0)
    {   // cp in [0x00000, 0x0007F], most likely case.
        cp = str[0];
        return str + 1;
    }
    if ((str[0] & 0xFF) >= 0xC2 && (str[0] & 0xFF) <= 0xDF && (str[1] & 0xC0) == 0x80)
    {   // cp in [0x00080, 0x007FF]
        cp = (str[0] & 0x1F) <<  6 | (str[1] & 0x3F);
        return str + 2;
    }
    if ((str[0] & 0xF0) == 0xE0 && (str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80)
    {   // cp in [0x00800, 0x0FFFF]
        cp = (str[0] & 0x0F) << 12 | (str[1] & 0x3F) << 6 | (str[2] & 0x3F);
        return str + 3;
    }
    if ((str[0] & 0xFF) == 0xF0 && (str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80 && (str[3] & 0xC0) == 0x80)
    {   // cp in [0x10000, 0x3FFFF]
        cp = (str[1] & 0x3F) << 12 | (str[2] & 0x3F) << 6 | (str[3] & 0x3F);
        return str + 4;
    }
    // else, invalid UTF-8 codepoint.
    cp = 0xFFFFFFFFU;
    return str + 1;
}

/*////////////////////////
//   Public Functions   //
////////////////////////*/
/// @summary Find the end of a volume and directory information portion of a path.
/// @param path The path string to search.
/// @param out_pathlen On return, indicates the number of bytes in the volume and
/// directory information of the path string. If the input path has no volume or
/// directory information, this value will be set to zero.
/// @param out_strlen On return, indicates the number of bytes in the input path,
/// not including the trailing zero byte.
/// @return A pointer to one past the last volume or directory separator, if present;
/// otherwise, the input pointer path.
public_function char const* pathend(char const *path, size_t &out_pathlen, size_t &out_strlen)
{
    if (path == NULL)
    {
        out_pathlen = 0;
        out_strlen  = 0;
        return path;
    }

    char        ch   = 0;
    char const *last = path;
    char const *iter = path;
    while ((ch = *iter++) != 0)
    {
        if (ch == ':' || ch == '\\' || ch == '/')
            last = iter;
    }
    out_strlen  = size_t(iter - path - 1);
    out_pathlen = size_t(last - path);
    return last;
}

/// @summary Find the extension part of a filename or path string.
/// @param path The path string to search; ideally just the filename portion.
/// @param out_extlen On return, indicates the number of bytes of extension information.
/// @return A pointer to the first character of the extension. Check the value of
/// out_extlen to be sure that there is extension information.
public_function char const* extpart(char const *path, size_t &out_extlen)
{
    if (path == NULL)
    {
        out_extlen = 0;
        return path;
    }

    char        ch    = 0;
    char const *last  = path;
    char const *iter  = path;
    while ((ch = *iter++) != 0)
    {
        if (ch == '.')
            last = iter;
    }
    if (last != path)
    {   // we found an extension separator somewhere in the input path.
        // @note: this also filters out the case of ex. path = '.gitignore'.
        out_extlen = size_t(iter - last - 1);
    }
    else
    {
        // no extension part is present in the input path.
        out_extlen = 0;
    }
    return last;
}

/// @summary Perform string matching with support for wildcards.
/// @param str The string to check.
/// @param filter The filter string, which may contain wildcards '?' and '*'.
/// The '?' character matches any string except an empty string, while '*'
/// matches any string including the empty string.
/// @return true if str matches the filter pattern.
public_function bool pathmatch(char const *str, char const *filter)
{   // TODO(rlk): Proper UTF-8 support.
    char    ch = 0;
    while ((ch = *filter) != 0)
    {
        if (ch == '?')
        {
            if (*str == 0) return false;
            ++filter;
            ++str;
        }
        else if (ch == '*')
        {
            if (pathmatch(str, filter + 1))
                return true;
            if (*str && pathmatch(str + 1, filter))
                return true;
            return false;
        }
        else
        {   // standard comparison of two characters, ignoring case.
            if (toupper(*str++) != toupper(*filter++))
                return false;
        }
    }
    return (*str == 0 && *filter == 0);
}

/// @summary Calculates a 32-bit hash value for a path string. Forward and
/// backslashes are treated as equivalent.
/// @param path A NULL-terminated UTF-8 path string.
/// @param out_end On return, points to one byte past the zero byte.
/// @return The hash of the specified string.
public_function uint32_t pathhash(char const *path, char const **out_end)
{
    if (path == NULL)
    {
        if (out_end) *out_end = NULL;
        return 0;
    }

    uint32_t    cp   = 0;
    uint32_t    cp2  = 0;
    uint32_t    hash = 0;
    char const *iter = next_utf8_codepoint(path, cp);
    while (cp != 0)
    {
        cp2    = cp != '\\' ? cp : '/'; // ignore separator differences
        hash   = _lrotl(hash, 7) + cp2; // rotate left 7 bits + cp2
        iter   = next_utf8_codepoint(iter, cp);
    }
    if (out_end)
    {
       *out_end = iter + 1;
    }
    return hash;
}

/// @summary Allocates resources for and initializes a new file list.
/// @param list The file list to initialize.
/// @param capacity The initial capacity, in number of paths.
/// @param path_bytes The total number of bytes to allocate for path data.
/// @return true if the file list was initialized successfully.
public_function bool create_file_list(file_list_t *list, size_t capacity, size_t path_bytes)
{
    if (list != NULL)
    {
        list->PathCapacity = uint32_t(capacity);
        list->PathCount    = 0;
        list->BlobCapacity = uint32_t(path_bytes);
        list->BlobCount    = 0;
        list->MaxPathBytes = 0;
        list->TotalBytes   = 0;
        list->HashList     = NULL;
        list->SizeList     = NULL;
        list->PathOffset   = NULL;
        list->PathData     = NULL;
        if (capacity > 0)
        {
            list->HashList    = (uint32_t *) malloc(capacity * sizeof(uint32_t));
            list->SizeList    = (uint32_t *) malloc(capacity * sizeof(uint32_t));
            list->PathOffset  = (uint32_t *) malloc(capacity * sizeof(uint32_t));
            list->TotalBytes +=  uint32_t          (capacity * sizeof(uint32_t) * 3);
        }
        if (path_bytes > 0)
        {
            list->PathData    = (char*) malloc(path_bytes * sizeof(char));
            list->TotalBytes +=  uint32_t     (path_bytes * sizeof(char));
        }
        return true;
    }
    else return false;
}

/// @summary Releases resources associated with a file list.
/// @param list The file list to delete.
public_function void delete_file_list(file_list_t *list)
{
    if (list != NULL)
    {
        if (list->PathData   != NULL) free(list->PathData);
        if (list->PathOffset != NULL) free(list->PathOffset);
        if (list->SizeList   != NULL) free(list->SizeList);
        if (list->HashList   != NULL) free(list->HashList);
        list->PathCapacity    = 0;
        list->PathCount       = 0;
        list->BlobCapacity    = 0;
        list->BlobCount       = 0;
        list->MaxPathBytes    = 0;
        list->TotalBytes      = 0;
        list->HashList        = NULL;
        list->SizeList        = NULL;
        list->PathOffset      = NULL;
        list->PathData        = NULL;
    }
}

/// @summary Ensures that the file list has a specified minimum capacity, and if not, grows.
/// @param list The file list to check and possibly grow.
/// @param capacity The minimum number of paths that can be stored.
/// @param path_bytes The minimum number of bytes for storing path data.
/// @return true if the file list has the specified capacity.
public_function bool ensure_file_list(file_list_t *list, size_t capacity, size_t path_bytes)
{
    if (list != NULL)
    {
        if (list->PathCapacity >= capacity && list->BlobCapacity >= path_bytes)
        {   // the list already meets the specified capacity; nothing to do.
            return true;
        }
        if (list->PathCapacity < capacity)
        {
            uint32_t *hash     = (uint32_t*) realloc(list->HashList  , capacity * sizeof(uint32_t));
            uint32_t *size     = (uint32_t*) realloc(list->SizeList  , capacity * sizeof(uint32_t));
            uint32_t *offset   = (uint32_t*) realloc(list->PathOffset, capacity * sizeof(uint32_t));
            if (hash   != NULL)  list->HashList    = hash;
            if (size   != NULL)  list->SizeList    = size;
            if (offset != NULL)  list->PathOffset  = offset;
            if (offset == NULL || size == NULL || hash == NULL)
            {
                return false;
            }
            list->TotalBytes  += uint32_t((capacity - list->PathCapacity) * sizeof(uint32_t) * 3);
            list->PathCapacity = uint32_t (capacity);
        }
        if (list->BlobCapacity < path_bytes)
        {
            char *blob = (char*) realloc(list->PathData, path_bytes * sizeof(char));
            if (blob  != NULL)   list->PathData = blob;
            else return false;
            list->TotalBytes  += uint32_t((path_bytes - list->BlobCapacity) * sizeof(char));
            list->BlobCapacity = uint32_t (path_bytes);
        }
        return true;
    }
    else return false;
}

/// @summary Appends an item to the file list, growing it if necessary.
/// @param list The file list to modify.
/// @param path A NULL-terminated UTF-8 file path to append.
public_function void append_file_list(file_list_t *list, char const *path)
{
    if (list->PathCount == list->PathCapacity)
    {   // need to grow the list of path attributes.
        size_t new_items = file_list_grow_size(list->PathCapacity, FILE_LIST_PATH_GROW_LIMIT, list->PathCapacity + 1);
        ensure_file_list(list, new_items, list->BlobCapacity);
    }

    char const  *endp = NULL;
    uint32_t     hash = pathhash(path, &endp);
    size_t       nb   = endp  - path - 1;
    if (list->BlobCount + nb >= list->BlobCapacity)
    {
        size_t new_bytes = file_list_grow_size(list->BlobCapacity, FILE_LIST_BLOB_GROW_LIMIT, list->BlobCount + nb);
        ensure_file_list(list, list->PathCapacity, new_bytes);
    }

    size_t  index = list->PathCount;
    uint32_t size = uint32_t(nb-1);
    // append the basic path properties to the list:
    list->HashList  [index] = hash;
    list->SizeList  [index] = size;
    list->PathOffset[index] = list->BlobCount;
    // intern the path string data (including zero byte):
    memcpy(&list->PathData[list->BlobCount], path, nb);
    list->BlobCount += uint32_t(nb);
    list->PathCount += 1;
    if (nb > list->MaxPathBytes)
    {   // includes the zero byte.
        list->MaxPathBytes = uint32_t(nb);
    }
}

/// @summary Resets a file list to empty without freeing any resources.
/// @param list The file list to reset.
public_function void clear_file_list(file_list_t *list)
{
    list->PathCount    = 0;
    list->BlobCount    = 0;
    list->MaxPathBytes = 0;
    list->TotalBytes   = 0;
}

/// @summary Retrieves a path string from a file list.
/// @param list The file list to query.
/// @param index The zero-based index of the path to retrieve.
/// @return A pointer to the start of the path string, or NULL.
public_function char const* file_list_path(file_list_t const *list, size_t index)
{
    return &list->PathData[list->PathOffset[index]];
}

/// @summary Searches for a given hash value within the file list.
/// @param list The file list to search.
/// @param hash The 32-bit unsigned integer hash of the search path.
/// @param start The zero-based starting index of the search.
/// @param out_index On return, this location is updated with the index of the
/// item within the list. This index is valid until the list is modified.
/// @return true if the item was found in the list.
public_function bool search_file_list_byhash(file_list_t const *list, uint32_t hash, size_t start, size_t &out_index)
{
    size_t   const  hash_count = list->PathCount;
    uint32_t const *hash_list  = list->HashList;
    for (size_t i = start; i < hash_count; ++i)
    {
        if (hash_list[i] == hash)
        {
            out_index = i;
            return true;
        }
    }
    return false;
}

/// @summary Locates a specific path within the file list.
/// @param list The file list to search.
/// @param path A NULL-terminated UTF-8 file path to search for.
/// @param out_index On return, this location is updated with the index of the
/// item within the list. This index is valid until the list is modified.
/// @return true if the item was found in the list.
public_function bool search_file_list_bypath(file_list_t const *list, char const *path, size_t &out_index)
{
    char const *end  = NULL;
    uint32_t   hash  = pathhash(path, &end);
    return search_file_list_byhash(list, hash, 0, out_index);
}

/// @summary Verifies a file list, ensuring there are no hash collisions.
/// @param list The file list to verify.
/// @return true if the list contains no hash collisions.
public_function bool verify_file_list(file_list_t const *list)
{
    size_t   const  hash_count = list->PathCount;
    uint32_t const *hash_list  = list->HashList;
    for (size_t i = 0; i < hash_count; ++i)
    {
        uint32_t path_hash = hash_list[i];
        size_t   num_hash  = 0; // number of items with hash path_hash
        for (size_t j = 0; j < hash_count; ++j)
        {
            if (path_hash == hash_list[j])
            {
                if (++num_hash > 1)
                    return false;
            }
        }
    }
    return true;
}

/// @summary Pretty-prints a file list to a buffered stream.
/// @param fp The output stream. This is typically stdout or stderr.
/// @param list The file list to format and write to the output stream.
public_function void format_file_list(FILE *fp, file_list_t const *list)
{
    fprintf(fp, " Index | Hash     | Length | Offset | Path\n");
    fprintf(fp, "-------+----------+--------+--------+-------------------------------------------\n");
    for (size_t i = 0; i < list->PathCount; ++i)
    {
        fprintf(fp, " %5u | %08X | %6u | %6u | %s\n",
                unsigned(i),
                list->HashList[i],
                list->SizeList[i],
                list->PathOffset[i],
                file_list_path(list, i));
    }
    fprintf(fp, "\n");
}

/// @summary Enumerates all files under a directory that match a particular filter.
/// The implementation of this function is platform-specific.
/// @param dest The file list to populate with the results. The list is not cleared by the call.
/// @param path The path to search. If this is an empty string, the CWD is searched.
/// @param filter The filter string used to accept filenames. The filter '*' accepts everything.
/// @param recurse Specify true to recurse into subdirectories.
/// @return true if enumeration completed without error.
public_function bool enumerate_files(file_list_t *dest, char const *path, char const *filter, bool recurse)
{
    char *cwd_buf  = NULL;
    if (path == NULL)
    {   // search the current working directory of the process.
        size_t nchars = (size_t) GetCurrentDirectoryA(0, NULL);
        if ((cwd_buf  = (char *) malloc(nchars * sizeof(char))) == NULL)
        {   // failed to allocate the necessary memory.
            return false;
        }
        // GetCurrentDirectoryA will NULL-terminate cwd_buf.
        GetCurrentDirectoryA((DWORD) nchars, cwd_buf);
    }

    char const *base_path  = path != NULL ? path : cwd_buf;
    size_t      dir_len    = strlen(base_path);
    size_t      path_len   = dir_len + 1 + MAX_PATH + 1;
    size_t      filter_len = dir_len + 3;
    char       *filterbuf  = (char*) malloc(filter_len); // <path>\*0
    char       *pathbuf    = (char*) malloc(path_len);   // <path>\<result>0

    if (pathbuf == NULL || filterbuf == NULL)
    {   // failed to allocate the necessary memory.
        free(cwd_buf);
        free(filterbuf);
        free(pathbuf);
        return false;
    }

    // generate a filter string of the form <path>\* so that we enumerate
    // all files and directories under the specified path, even if 'path'
    // doesn't directly contain any files matching the extension filter.
    strncpy(filterbuf, base_path, dir_len);
    if (base_path[dir_len - 1] != '\\' && base_path[dir_len - 1] != '/')
    {
        filterbuf[dir_len + 0] = '\\';
        filterbuf[dir_len + 1] = '*';
        filterbuf[dir_len + 2] = '\0';
    }
    else
    {
        filterbuf[dir_len + 0] = '*';
        filterbuf[dir_len + 1] = '\0';
    }

    // open the find for all files and directories directly under 'path'.
    FINDEX_INFO_LEVELS  l = FINDEX_INFO_LEVELS (1); // FindExInfoBasic
    FINDEX_SEARCH_OPS   s = FINDEX_SEARCH_OPS  (0); // FindExSearchNameMatch
    WIN32_FIND_DATAA info = {0};
    HANDLE       find_obj = FindFirstFileExA(filterbuf, l, &info, s, NULL, 0);
    if (find_obj != INVALID_HANDLE_VALUE)
    {
        strncpy(pathbuf, base_path, dir_len);
        if (base_path[dir_len - 1] != '\\' && base_path[dir_len - 1] != '/')
        {   // append a trailing '\\'.
            pathbuf[dir_len++] = '\\';
        }
        do
        {
            if (0 == strcmp(info.cFileName, "."))
                continue;
            if (0 == strcmp(info.cFileName, ".."))
                continue;

            strcpy(&pathbuf[dir_len], info.cFileName);
            if (recurse && (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {   // recurse into the subdirectory.
                enumerate_files(dest, pathbuf, filter, true);
            }
            if ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
                (info.dwFileAttributes & FILE_ATTRIBUTE_DEVICE   ) == 0 &&
                (info.dwFileAttributes & FILE_ATTRIBUTE_TEMPORARY) == 0 &&
                (info.dwFileAttributes & FILE_ATTRIBUTE_VIRTUAL  ) == 0)
            {   // this is a regular file. if it passes the filter, add it.
                if (pathmatch(info.cFileName, filter))
                {
                    append_file_list(dest, pathbuf);
                }
            }
        } while (FindNextFileA(find_obj, &info));
        FindClose(find_obj);
        free(cwd_buf);
        free(filterbuf);
        free(pathbuf);
        return true;
    }
    else
    {   // couldn't open find handle.
        free(cwd_buf);
        free(filterbuf);
        free(pathbuf);
        return false;
    }
}

/// @summary Generates a unique filename for use as a temp file in a given directory.
/// @param path The NULL-terminated UTF-8 string specifying the target path.
/// @param prefix The NULL-terminated UCS-2 string specifying the filename prefix.
/// @return A string <volume and directory info from path>\<prefix>-########\0.
/// The returned string should be freed using the standard C library free() function.
public_function WCHAR* make_temp_path(char const *path, WCHAR const *prefix)
{
    int    nbytes =-1;    // the number of bytes of volume and directory info
    int    nchars = 0;
    size_t pathlen= 0;    // the number of bytes in path, not including zero-byte
    size_t dirlen = 0;    // the number of bytes of volume and directory info
    size_t pfxlen = 0;    // the number of chars in prefix
    WCHAR *temp   = NULL; // the temporary file path we will return
    WCHAR  random[9];     // zero-terminated string of 8 random hex digits

    // ensure that a prefix is specified.
    if (prefix == NULL)
    {   // use the prefix 'tempfile'.
        prefix  = L"tempfile";
    }

    // the temp file should be created in the same directory as the output path.
    // this avoids problems with file moves across volumes or partitions, which
    // will either fail, or are implemented as a non-atomic file copy. the path
    // we return will contain the full volume and directory information from the
    // input path, and append a filename of the form prefix-######## to it.
    pathend(path, dirlen, pathlen);          // get the path portion of the input string.
    pfxlen = wcslen(prefix);                 // get the length of the prefix string.
    nbytes = dirlen > 0 ? int(dirlen)  : -1; // get the number of bytes of volume and directory info.
    nchars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, nbytes, NULL, 0);
    if (dirlen > 0 && nchars == 0)
    {   // the path cannot be converted from UTF-8 to UCS-2.
        return NULL;
    }
    // allocate storage for the path string, converted to UCS-2.
    // the output path has the form:
    // <volume and directory info from path>\<prefix>-########\0
    nchars =  int(nchars + 1 + pfxlen + 10);
    temp   = (WCHAR*) malloc(nchars * sizeof(WCHAR));
    if (temp == NULL) return NULL;
    memset(temp, 0, nchars * sizeof(WCHAR));
    if (dirlen > 0)
    {   // the output path has volume and directory information specified.
        // convert this data from UTF-8 to UCS-2 and append a directory separator.
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, int(dirlen), temp, nchars) == 0)
        {   // unable to perform the conversion. since we did this once previously,
            // to get the buffer length, this should not ever really happen.
            free(temp);
            return NULL;
        }
        wcscat(temp, L"\\");
    }
    wcscat(temp, prefix);
    wcscat(temp, L"-");

    // generate a 32-bit 'random' value and convert it to an eight-digit hex value.
    // start with the current tick count, and then mix the bits using the 4-byte
    // integer hash, full avalanche method from burtleburtle.net/bob/hash/integer.html.
    // add in the value of the processor time stamp counter in case we're doing this
    // in a tight loop. this helps prevent (but does not eliminate) filename collisions.
    uint32_t bits = uint32_t(__rdtsc());// GetTickCount();
    bits  = (bits + 0x7ed55d16) + (bits << 12);
    bits  = (bits ^ 0xc761c23c) ^ (bits >> 19);
    bits  = (bits + 0x165667b1) + (bits <<  5);
    bits  = (bits + 0xd3a2646c) ^ (bits <<  9);
    bits  = (bits + 0xfd7046c5) + (bits <<  3);
    bits  = (bits ^ 0xb55a4f09) ^ (bits >> 16);
    swprintf(random, 9, L"%08x" ,  bits);
    random[8] = 0;
    wcscat(temp, random);
    return temp;
}
