#ifndef _JSONV_TABLE_H
#define _JSONV_TABLE_H
#define T Table
typedef struct T T;

typedef struct TableEntry {
    void *key;
    void *value;
} TableEntry;

/*
 * @description  Instanciate a new table
 * @param hint   Initial size of the table
 * @param cmp    Keys comparaison function, defaults to atom comparaison
 * @param hash   An optional hash function
 * @return       Instance of a table
 */
T* table_new(int hint, int cmp(const void *x, const void *y),
            unsigned hash(const void *key));
/*
 * @description  Free a table (does not free items)
 * @param table  Instance of a table
 */
void table_free(T **table, void apply(void *key, void *value));

/*
 * @description  Retrieve the number of items in the table
 * @param table  Instance of a table
 * @return       Table length
 */
int table_length(T* table);

/*
 * @description  Add a new item to a table
 * @param table  Instance of a table
 * @param key    The key used to index the item
 * @param value  The item to store
 * @return       the previous record, if it exists with the given key or NULL
 */
void *table_put(T* table, const void *key, void *value);

/*
 * @description  Retrieve a specif item in the table
 * @param table  Instance of a table
 * @key          The item key
 * @return       the item if found or NULL
 */
void *table_get(T* table, const void *key);

/*
 * @description  Remove an item from the table
 * @param table  Instance of a table
 * @param key    The item key
 * @return       The item to remove
 */
void *table_remove(T* table, const void *key);

/*
 * @description  Iterate over each item of the table
 * @note It is not permitted to mutate a table while iterating over it
 * @param table  Instance of a table
 * @param apply  A function to execute on every item 
 * @param cl     Apply function optional payload
 */
void table_map(T* table, void apply(const void *key, void **value, void *cl),
               void *cl);
/*
 * @description  Transform a table into an array
 * @param table  Instance of a table
 * @return       An array
 */
TableEntry *table_to_array(T *table, int *count);

/*
 * @description  Fuse two table together
 * @note         1. If case of duplicated keys, table b overwrite table a
 *               2. It is expected that both tables use same kind of keys
 *               3. It is expected that both tables use same hash function
 * @param a      First table
 * @param b      Second table
 * @return       A new table
 */
T *table_fuse(T *a, T *b);
#undef T
#endif
