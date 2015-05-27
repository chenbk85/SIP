/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines a structure for mapping user-defined identifiers to some 
/// value. Internally, a hash table is used for fast lookup. The table is not 
/// safe for concurrent access by multiple threads.
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
#define ID_BUCKET_SIZE      0U

/// @summary The array index used to store the capacity of a bucket.
#define ID_BUCKET_CAPACITY  1U

/// @summary The offset of the first key from the start of a bucket.
#define ID_KEY_OFFSET       2U

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Define a table mapping an externally-defined identifier to some 
/// type of value. Minimally, this is an unordered array, but for larger numbers
/// of identifiers, it makes sense to use multiple buckets, turning this into 
/// an array hash table with unsigned integer keys.
struct id_table_t
{
    size_t      NBuckets;  /// The number of hash buckets defined for the table.
    size_t      MaskBits;  /// The bit mask used to map a hashed key to a bucket index.
    uintptr_t **KBuckets;  /// The array of identifier (key) buckets.
    uintptr_t **VBuckets;  /// The array of value buckets.
};

/*///////////////
//   Globals   //
///////////////*/

/*///////////////////////
//   Local Functions   //
///////////////////////*/

/*////////////////////////
//   Public Functions   //
////////////////////////*/
/// @summary Initializes an empty table, allocating storage for the bucket lists.
/// @param table The table to initialize.
/// @param bucket_count The number of buckets to allocate for the table.
public_function void id_table_create(id_table_t *table, size_t bucket_count=1)
{
    table->NBuckets = next_pow2(bucket_count);
    table->MaskBits = table->NBuckets - 1;
    table->KBuckets = (uintptr_t**) malloc(table->NBuckets * sizeof(uintptr_t*));
    table->VBuckets = (uintptr_t**) malloc(table->NBuckets * sizeof(uintptr_t*));
    for (size_t   i = 0; i < table->NBuckets; ++i)
    {
        table->KBuckets[i] = NULL;
        table->VBuckets[i] = NULL;
    }
}

/// @summary Releases all memory associated with an ID table.
/// @param table The table to free.
public_function void id_table_delete(id_table_t *table)
{
    for (size_t i = 0; i < table->NBuckets; ++i)
    {
        if (table->KBuckets[i]) free(table->KBuckets[i]);
        if (table->VBuckets[i]) free(table->VBuckets[i]);
    }
    if (table->KBuckets) free(table->KBuckets);
    if (table->VBuckets) free(table->VBuckets);
    table->NBuckets = 0;
    table->KBuckets = NULL;
    table->VBuckets = NULL;
}

/// @summary Retrieves the value associated with an identifier.
/// @param table The table to query.
/// @param id The identifier to associated with the value to retrieve.
/// @param out On return, this location is updated with the associated value.
/// @return true if a value is associated with the given identifier.
public_function bool id_table_get(id_table_t *table, uintptr_t id, uintptr_t *out)
{
    uintptr_t bucket = mix_bits(id) & table->MaskBits;
    if (table->KBuckets[bucket] != NULL)
    {
        uintptr_t *k = table->KBuckets[bucket];
        uintptr_t  n = k[ID_BUCKET_SIZE];
        for (uintptr_t ki = ID_KEY_OFFSET, vi = 0; vi < n; ++ki, ++vi)
        {
            if (k[ki] == id)
            {
                *out = table->VBuckets[bucket][vi];
                return true;
            }
        }
    }
    return false;
}

/// @summary Inserts an item into the table. No check is performed for duplicate entries. It is assumed that the caller knows that the specified identifier does not currently exist in the table.
/// @param table The table to update.
/// @param id The identifier to insert.
/// @param value The value associated with the given identifier.
public_function void id_table_put(id_table_t *table, uintptr_t id, uintptr_t value)
{
    uintptr_t bucket = mix_bits(id) & table->MaskBits;
    if (table->KBuckets[bucket] != NULL)
    {   // insert into the existing bucket, resizing if necessary.
        uintptr_t *k = table->KBuckets[bucket];
        uintptr_t *v = table->VBuckets[bucket];
        uintptr_t  n = k[ID_BUCKET_SIZE];
        uintptr_t  m = k[ID_BUCKET_CAPACITY];
        if (n == m)
        {   // grow bucket capacity by doubling.
            m = (m * 2);
            k = (uintptr_t*) realloc(k, (m + 2) * sizeof(uintptr_t));
            v = (uintptr_t*) realloc(v, (m    ) * sizeof(uintptr_t));
            k[ID_BUCKET_SIZE]      = n;
            k[ID_BUCKET_CAPACITY]  = m;
            table->KBuckets[bucket]= k;
            table->VBuckets[bucket]= v;
        }
        k[ID_BUCKET_SIZE]++;
        k[ID_KEY_OFFSET+n] = id;
        v[n] = value;
    }
    else
    {   // allocate storage for a new bucket and insert.
        table->KBuckets[bucket] = (uintptr_t*) malloc(10 * sizeof(uintptr_t));
        table->KBuckets[bucket][ID_BUCKET_SIZE]     = 1;
        table->KBuckets[bucket][ID_BUCKET_CAPACITY] = 8;
        table->KBuckets[bucket][ID_KEY_OFFSET]      = id;
        table->VBuckets[bucket] = (uintptr_t*) malloc(8  * sizeof(uintptr_t));
        table->VBuckets[bucket][0] = value;
    }
}

/// @summary Updates an item within the table, inserting it if it does not exist.
/// @param table The table to update.
/// @param id The identifier to update or insert.
/// @param new_value The value to be associated with the identifier.
/// @param old_value On return, this location is updated with the previous value.
/// @return true if an update was performed, or false if an insert was performed.
public_function bool id_table_update(id_table_t *table, uintptr_t id, uintptr_t new_value, uintptr_t *old_value)
{
    uintptr_t bucket = mix_bits(id) & table->MaskBits;
    if (table->KBuckets[bucket] != NULL)
    {   // insert into the existing bucket, resizing if necessary.
        uintptr_t *k = table->KBuckets[bucket];
        uintptr_t *v = table->VBuckets[bucket];
        uintptr_t  n = k[ID_BUCKET_SIZE];
        uintptr_t  m = k[ID_BUCKET_CAPACITY];
        // search for the ID and perform an update if it exists.
        for (uintptr_t ki = ID_KEY_OFFSET, vi = 0; vi < n; ++ki, ++vi)
        {
            if (k[ki] == id)
            {
                if (old_value) *old_value = v[vi];
                v[vi] = new_value;
                return true;
            }
        }
        // the ID wasn't found, so perform a table insert.
        if (n == m)
        {   // grow the table by doubling.
            m = (m * 2);
            k = (uintptr_t*) realloc(k, (m + 2) * sizeof(uintptr_t));
            v = (uintptr_t*) realloc(v, (m    ) * sizeof(uintptr_t));
            k[ID_BUCKET_SIZE]      = n;
            k[ID_BUCKET_CAPACITY]  = m;
            table->KBuckets[bucket]= k;
            table->VBuckets[bucket]= v;
        }
        k[ID_BUCKET_SIZE]++;
        k[ID_KEY_OFFSET+n] = id;
        v[n] = new_value;
        return false;
    }
    else
    {   // allocate storage for a new bucket and insert.
        table->KBuckets[bucket] = (uintptr_t*) malloc(10 * sizeof(uintptr_t));
        table->KBuckets[bucket][ID_BUCKET_SIZE]     = 1;
        table->KBuckets[bucket][ID_BUCKET_CAPACITY] = 8;
        table->KBuckets[bucket][ID_KEY_OFFSET]      = id;
        table->VBuckets[bucket] = (uintptr_t*) malloc(8  * sizeof(uintptr_t));
        table->VBuckets[bucket][0] = new_value;
        return false;
    }
}

/// @summary Removes an identifier from the table.
/// @param table The table to update.
/// @param id The identifier to remove from the table.
/// @param out On return, this location is updated with the associated value.
/// @return true if the value was removed.
public_function bool id_table_remove(id_table_t *table, uintptr_t id, uintptr_t *out)
{
    uintptr_t bucket = mix_bits(id) & table->MaskBits;
    if (table->KBuckets[bucket] != NULL)
    {
        uintptr_t *k = table->KBuckets[bucket];
        uintptr_t *v = table->VBuckets[bucket];
        uintptr_t  n = k[ID_BUCKET_SIZE];
        for (uintptr_t ki = ID_KEY_OFFSET, vi = 0; vi < n; ++ki, ++vi)
        {
            if (k[ki]  == id)
            {
                uintptr_t p = v[vi];
                v[vi]   = v[n - 1];
                k[ki]   = k[ID_KEY_OFFSET + n - 1];
                k[ID_BUCKET_SIZE] = n - 1;
                if (out != NULL) *out = p;
                return true;
            }
        }
    }
    return false;
}

/// @summary Removes all items from the table, but does not free any memory.
/// @param table The table to clear.
public_function void id_table_clear(id_table_t *table)
{
    for (size_t i = 0; i < table->NBuckets; ++i)
    {
        if (table->KBuckets[i] != NULL)
        {
            table->KBuckets[i][ID_BUCKET_SIZE] = 0;
        }
    }
}
