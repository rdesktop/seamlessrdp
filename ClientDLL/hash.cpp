#include <string.h>
#include <stdlib.h> 
/* #define NDEBUG */
#include <assert.h>

#include "hash.h"


/*
** public domain code by Jerry Coffin.
**
** Tested with Visual C 1.0 and Borland C 3.1.
** Compiles without warnings, and seems like it should be pretty
** portable.
*/

/* HW: HenkJan Wolthuis, 1997, public domain
 
      changed functionnames, all public functions now have a 'hash_' prefix
      minor editing, marked 'm all(?) with a description
      removed a bug in hash_del and one in hash_enumerate
      added some assertions
      added a 'count' member to hold the number of elements
      added hash_sorted_enum, sometimes useful
      changed the testmain
*/

/*
** RBS: Bob Stout, 2003, public domain
**
**  1. Fixed some problems in hash() static function.
**  2. Use unsigned shorts for hash values. This was implicit in the original
**     which was written for PC's using early Microsoft and Borland compilers.
*/

/* HW: #define to allow duplicate keys, they're added before the existing
      key so hash_lookup finds the last one inserted first (LIFO)
      when not defined, hash_insert swaps the datapointers, returning a
      pointer to the old data
*/ 
/* #define DUPLICATE_KEYS */

/*
** These are used in freeing a table.  Perhaps I should code up
** something a little less grungy, but it works, so what the heck.
 */
static void ( *function ) ( void * ) = NULL;
static hash_table *the_table = NULL;


/* Initialize the hash_table to the size asked for.  Allocates space
** for the correct number of pointers and sets them to NULL.  If it
** can't allocate sufficient memory, signals error by setting the size
** of the table to 0.
*/ 
/*HW: changed, now returns NULL on malloc-failure */
hash_table *hash_construct_table( hash_table * table, size_t size )
{
    size_t i;
    bucket **temp;
    
    table->size = size;
    table->count = 0;
    table->table = ( bucket ** ) malloc( sizeof( bucket * ) * size );
    temp = table->table;
    
    if ( NULL == temp ) {
        table->size = 0;
        return NULL;            /*HW: was 'table' */
    }
    
    for ( i = 0; i < size; i++ )
        temp[ i ] = NULL;
        
    return table;
}


/*
** Hashes a string to produce an unsigned short, which should be
** sufficient for most purposes.
** RBS: fixed per user feedback from Steve Greenland
*/

static unsigned short hash( char *string )
{
    unsigned short ret_val = 0;
    int i;
    
    while ( *string ) {
        /*
         ** RBS: Added conditional to account for strings in which the
         ** length is less than an integral multiple of sizeof(int).
         **
         ** Note: This fixes the problem of hasing trailing garbage, but
         ** doesn't fix the problem with some CPU's which can't align on
         ** byte boundries. Any decent C compiler *should* fix this, but
         ** it still might extract a performance hit. Also unaddressed is
         ** what happens when using a CPU which addresses data only on
         ** 4-byte boundries when it tries to work with a pointer to a
         ** 2-byte unsigned short.
         */
        
        if ( strlen( string ) >= sizeof( unsigned short ) )
            i = *( unsigned short * ) string;
        else
            i = ( unsigned short ) ( *string );
        ret_val ^= i;
        ret_val <<= 1;
        string++;
    }
    return ret_val;
}

/*
** Insert 'key' into hash table.
** Returns pointer to old data associated with the key, if any, or
** NULL if the key wasn't in the table previously.
*/ 
/* HW: returns NULL if malloc failed */
void *hash_insert( char *key, void *data, hash_table * table )
{
    unsigned short val = hash( key ) % table->size;
    bucket *ptr;
    
    assert( NULL != key );
    
    /*
     ** NULL means this bucket hasn't been used yet.  We'll simply
     ** allocate space for our new bucket and put our data there, with
     ** the table pointing at it.
     */
    
    if ( NULL == ( table->table ) [ val ] ) {
        ( table->table ) [ val ] = ( bucket * ) malloc( sizeof( bucket ) );
        if ( NULL == ( table->table ) [ val ] )
            return NULL;
            
        if ( NULL ==
                ( ( table->table ) [ val ] ->key = ( char * ) malloc( strlen( key ) + 1 ) ) ) {
            free( ( table->table ) [ val ] );
            ( table->table ) [ val ] = NULL;
            return NULL;
        }
        strcpy( ( table->table ) [ val ] ->key, key );
        ( table->table ) [ val ] ->next = NULL;
        ( table->table ) [ val ] ->data = data;
        table->count++;         /* HW */
        return ( table->table ) [ val ] ->data;
    }
    
    /* HW: added a #define so the hashtable can accept duplicate keys */
#ifndef DUPLICATE_KEYS
    /*
     ** This spot in the table is already in use.  See if the current string
     ** has already been inserted, and if so, increment its count.
     */ /* HW: ^^^^^^^^ ?? */
    for ( ptr = ( table->table ) [ val ]; NULL != ptr; ptr = ptr->next )
        if ( 0 == strcmp( key, ptr->key ) ) {
            void * old_data;
            
            old_data = ptr->data;
            ptr->data = data;
            return old_data;
        }
#endif
    /*
     ** This key must not be in the table yet.  We'll add it to the head of
     ** the list at this spot in the hash table.  Speed would be
     ** slightly improved if the list was kept sorted instead.  In this case,
     ** this code would be moved into the loop above, and the insertion would
     ** take place as soon as it was determined that the present key in the
     ** list was larger than this one.
     */
    
    ptr = ( bucket * ) malloc( sizeof( bucket ) );
    if ( NULL == ptr )
        return NULL;            /*HW: was 0 */
        
    if ( NULL == ( ptr->key = ( char * ) malloc( strlen( key ) + 1 ) ) ) {
        free( ptr );
        return NULL;
    }
    strcpy( ptr->key, key );
    ptr->data = data;
    ptr->next = ( table->table ) [ val ];
    ( table->table ) [ val ] = ptr;
    table->count++;             /* HW */
    
    return data;
}


/*
** Look up a key and return the associated data.  Returns NULL if
** the key is not in the table.
*/
void *hash_lookup( char *key, hash_table * table )
{
    unsigned short val = hash( key ) % table->size;
    bucket *ptr;
    
    assert( NULL != key );
    
    if ( NULL == ( table->table ) [ val ] )
        return NULL;
        
    for ( ptr = ( table->table ) [ val ]; NULL != ptr; ptr = ptr->next ) {
        if ( 0 == strcmp( key, ptr->key ) )
            return ptr->data;
    }
    
    return NULL;
}

/*
** Delete a key from the hash table and return associated
** data, or NULL if not present.
*/

void *hash_del( char *key, hash_table * table )
{
    unsigned short val = hash( key ) % table->size;
    void *data;
    bucket *ptr, *last = NULL;
    
    assert( NULL != key );
    
    if ( NULL == ( table->table ) [ val ] )
        return NULL;            /* HW: was 'return 0' */
        
    /*
     ** Traverse the list, keeping track of the previous node in the list.
     ** When we find the node to delete, we set the previous node's next
     ** pointer to point to the node after ourself instead.      We then delete
     ** the key from the present node, and return a pointer to the data it
     ** contains.
     */
    for ( last = NULL, ptr = ( table->table ) [ val ];
            NULL != ptr; last = ptr, ptr = ptr->next ) {
        if ( 0 == strcmp( key, ptr->key ) ) {
            if ( last != NULL ) {
                data = ptr->data;
                last->next = ptr->next;
                free( ptr->key );
                free( ptr );
                table->count--; /* HW */
                return data;
            }
            
            /* If 'last' still equals NULL, it means that we need to
             ** delete the first node in the list. This simply consists
             ** of putting our own 'next' pointer in the array holding
             ** the head of the list. We then dispose of the current
             ** node as above.
             */
            else {
                /* HW: changed this bit to match the comments above */
                data = ptr->data;
                ( table->table ) [ val ] = ptr->next;
                free( ptr->key );
                free( ptr );
                table->count--; /* HW */
                return data;
            }
        }
    }
    
    /*
     ** If we get here, it means we didn't find the item in the table.
     ** Signal this by returning NULL.
     */
    
    return NULL;
}

/*
** free_table iterates the table, calling this repeatedly to free
** each individual node.  This, in turn, calls one or two other
** functions - one to free the storage used for the key, the other
** passes a pointer to the data back to a function defined by the user,
** process the data as needed.
*/

static void free_node( char *key, void *data )
{
    ( void ) data;
    
    assert( NULL != key );
    
    if ( NULL != function ) {
        function( hash_del( key, the_table ) );
    } else
        hash_del( key, the_table );
}

/*
** Frees a complete table by iterating over it and freeing each node.
** the second parameter is the address of a function it will call with a
** pointer to the data associated with each node.  This function is
** responsible for freeing the data, or doing whatever is needed with
** it.
*/

void hash_free_table( hash_table * table, void ( *func ) ( void * ) )
{
    function = func;
    the_table = table;
    
    hash_enumerate( table, free_node );
    free( table->table );
    table->table = NULL;
    table->size = 0;
    table->count = 0;           /* HW */
    
    the_table = NULL;
    function = NULL;
}

/*
** Simply invokes the function given as the second parameter for each
** node in the table, passing it the key and the associated data.
*/

void hash_enumerate( hash_table * table, void ( *func ) ( char *, void * ) )
{
    unsigned i;
    bucket *temp;
    bucket *swap;
    
    for ( i = 0; i < table->size; i++ ) {
        if ( NULL != ( table->table ) [ i ] ) {
            /* HW: changed this loop */
            temp = ( table->table ) [ i ];
            while ( NULL != temp ) {
                /* HW: swap trick, in case temp is freed by 'func' */
                swap = temp->next;
                func( temp->key, temp->data );
                temp = swap;
            }
        }
    }
}

/*      HW: added hash_sorted_enum()
 
      hash_sorted_enum is like hash_enumerate but gives
      sorted output. This is much slower than hash_enumerate, but
      sometimes nice for printing to a file...
*/

typedef struct sort_struct
{
    char *key;
    void *data;
}
sort_struct;
static sort_struct *sortmap = NULL;

static int counter = 0;

/* HW: used as 'func' for hash_enumerate */
static void key_get( char *key, void *data )
{
    sortmap[ counter ].key = key;
    sortmap[ counter ].data = data;
    counter++;
}

/* HW: used for comparing the keys in qsort */
static int key_comp( const void *a, const void *b )
{
    return strcmp( ( *( sort_struct * ) a ).key, ( *( sort_struct * ) b ).key );
}

/*    HW: it's a compromise between speed and space. this one needs
      table->count * sizeof( sort_struct) memory.
      Another approach only takes count*sizeof(char*), but needs
      to hash_lookup the data of every key after sorting the key.
      returns 0 on malloc failure, 1 on success
*/
int hash_sorted_enum( hash_table * table, void ( *func ) ( char *, void * ) )
{
    int i;
    
    /* nothing to do ! */
    if ( NULL == table || 0 == table->count || NULL == func )
        return 0;
        
    /* malloc an pointerarray for all hashkey's and datapointers */
    if ( NULL ==
            ( sortmap =
                  ( sort_struct * ) malloc( sizeof( sort_struct ) * table->count ) ) )
        return 0;
        
    /* copy the pointers to the hashkey's */
    counter = 0;
    hash_enumerate( table, key_get );
    
    /* sort the pointers to the keys */
    qsort( sortmap, table->count, sizeof( sort_struct ), key_comp );
    
    /* call the function for each node */
    for ( i = 0; i < abs( ( table->count ) ); i++ ) {
        func( sortmap[ i ].key, sortmap[ i ].data );
    }
    
    /* free the pointerarray */
    free( sortmap );
    sortmap = NULL;
    
    return 1;
}

/* HW: changed testmain */
#define TEST
#ifdef TEST

#include <stdio.h> 
//#include "snip_str.h" /* for strdup() */

FILE *o;

void fprinter( char *string, void *data )
{
    fprintf( o, "%s:    %s\n", string, ( char * ) data );
}

void printer( char *string, void *data )
{
    printf( "%s:    %s\n", string, ( char * ) data );
}

/* function to pass to hash_free_table */
void strfree( void *d )
{
    /* any additional processing goes here (if you use structures as data) */
    /* free the datapointer */
    free( d );
}


int main( void )
{
    hash_table table;
    
    char *strings[] = {
                          "The first string",
                          "The second string",
                          "The third string",
                          "The fourth string",
                          "A much longer string than the rest in this example.",
                          "The last string",
                          NULL
                      };
                      
    char *junk[] = {
                       "The first data",
                       "The second data",
                       "The third data",
                       "The fourth data",
                       "The fifth datum",
                       "The sixth piece of data"
                   };
                   
    int i;
    void *j;
    
    hash_construct_table( &table, 211 );
    
    /* I know, no checking on strdup ;-)), but using strdup
       to demonstrate hash_table_free with a functionpointer */
    for ( i = 0; NULL != strings[ i ]; i++ )
        hash_insert( strings[ i ], strdup( junk[ i ] ), &table );
        
    /* enumerating to a file */
    if ( NULL != ( o = fopen( "HASH.HSH", "wb" ) ) ) {
        fprintf( o, "%d strings in the table:\n\n", table.count );
        hash_enumerate( &table, fprinter );
        fprintf( o, "\nsorted by key:\n" );
        hash_sorted_enum( &table, fprinter );
        fclose( o );
    }
    
    /* enumerating to screen */
    hash_sorted_enum( &table, printer );
    printf( "\n" );
    
    /* delete 3 strings, should be 3 left */
    for ( i = 0; i < 3; i++ ) {
        /* hash_del returns a pointer to the data */
        strfree( hash_del( strings[ i ], &table ) );
    }
    hash_enumerate( &table, printer );
    
    for ( i = 0; NULL != strings[ i ]; i++ ) {
        j = hash_lookup( strings[ i ], &table );
        if ( NULL == j )
            printf( "\n'%s' is not in the table", strings[ i ] );
        else
            printf( "\n%s is still in the table.", strings[ i ] );
    }
    
    hash_free_table( &table, strfree );
    
    return 0;
}
#endif /* TEST */
