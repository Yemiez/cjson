#ifndef CJSON_H
#define CJSON_H
#include <stdlib.h>

typedef enum {
    cjson_error_code_ok = 0,
    cjson_error_code_oom = 1000, // used_memory > highest_memory_usage
    cjson_error_code_alloc, // internal alloc returned NULL

    // syntax
    cjson_error_code_syntax_unexpected_eof = 2000,
    cjson_error_code_syntax_invalid_number,
    cjson_error_code_syntax_multiple_root_nodes,
    cjson_error_code_syntax_unexpected_comma,
    cjson_error_code_syntax_expected_key,
    cjson_error_code_syntax_expected_colon,
	cjson_error_code_syntax_unclosed_value,
} cjson_error_code_type;

typedef struct cjson_settings {
    void* (*mem_alloc)(size_t);
    void (*mem_free)(void*);
#ifdef CJSON_ENABLE_MULTITHREAD_SUPPORT
    void* mtx;
    int multithreaded;
#endif
#ifdef CJSON_ENABLE_MEMORY_LOGGING
    size_t memory_limit;
    size_t used_memory;
    size_t highest_memory_usage;
#endif
    size_t errc;
	
	// allows for permissive parsing, i.e. closing '[1, 2, {"key": "value"' will parse just fine,
	// and act as if both the array and object were properly closed.
	int permissive; // default = 0 (meaning errors will be raised).
} cjson_settings;

typedef enum {
    cjson_invalid = 1 << 0, // invalid (internal only, should never be returned from cjson_parse variants)
    cjson_kv = 1 << 1, // key-value, internal only for objects. string = key, child = value.
    cjson_string = 1 << 2, // string
    cjson_number = 1 << 3, // number
    cjson_object = 1 << 4, // object
    cjson_array = 1 << 5, // array
    cjson_boolean = 1 << 6, // boolean
    cjson_null = 1 << 7, // null
	cjson_integer = 1 << 8, // integer (& number)
	cjson_double = 1 << 9 // double (& number)
} cjson_type_flags;

// Internal struct used in parsing
typedef struct {
    size_t row; // row, 0-based
    size_t col; // column, 0-based
    size_t ofs; // index into buffer
} cjson_pos;

// You should never directly access the fields inside here.
typedef struct __cjson_value {
    struct __cjson_value* prev; // previous element (e.g previous array element, or previous key-value holder for objects)
    struct __cjson_value* next; // next element (e.g --__--)
    struct __cjson_value* child; // pointer to first child element or keyvalue.
    struct __cjson_value* childtail; // cached value, used in parsing stage to make appending children faster..
    int flags; // type flags
    char* string; // string value
    double doubleval; // double value
    int intval; // integer value.
} cjson_value;

// Initializes cjson lib with some basic settings. It is not necessary to call this function.
void cjson_init(cjson_settings*);
void cjson_set_permissive(int permissive);

#ifdef CJSON_ENABLE_MEMORY_LOGGING
// Prints some basic memory statistics (maximum memory in use at a single point, and current use).
void cjson_print_mem(void);
#endif

// Shuts down the CJSON library. It should always be called to prevent cjson_settings* from leaking.
void cjson_shutdown(void); 

// Parses a JSON file into a cjson_value. Returns NULL on failure.
cjson_value* cjson_parse_file(const char* filename);
// Parses a JSON string into a cjson_value. Returns NULL on failure.
cjson_value* cjson_parse(const char* buffer);
// Frees a cjson_value. Must always be called on the return value from cjson_parse variants.
void cjson_free_value(cjson_value*);
// Returns the current error code (if no error then cjson_error_code_ok is returned)
int cjson_error_code(void);
// Returns a friendlier message of the current error code.
const char* cjson_error_string(void);

// Returns a newly created empty object.
cjson_value* cjson_create_object();
// Returns a newly created empty array.
cjson_value* cjson_create_array();
// Returns a newly created NULL value.
cjson_value* cjson_create_null();
// Returns a new boolean value initialized with value.
cjson_value* cjson_create_boolean(int value); 
// Returns a new number value initialized to an integer value.
cjson_value* cjson_create_int(int value);
// Returns a new number value initialized to a double value.
cjson_value* cjson_create_double(double value);
// Returns a new string value initialized to string (note: the string is ALWAYS copied).
cjson_value* cjson_create_string(const char* string);

// Returns 1 if the value is a string.
int cjson_is_string(cjson_value*);
// Returns 1 if the value is a number.
int cjson_is_number(cjson_value*);
// Returns 1 if the value is a double.
int cjson_is_double(cjson_value*);
// Returns 1 if the value is an integer.
int cjson_is_integer(cjson_value*);
// Returns 1 if the value is an object.
int cjson_is_object(cjson_value*);
// Returns 1 if the value is a boolean.
int cjson_is_boolean(cjson_value*);
// Returns 1 if the value is null.
int cjson_is_null(cjson_value*);

// Returns the stringval of the value (must first check if it is string).
const char* cjson_get_string(cjson_value*);

// Returns the doubleval of the value (must first check if it is double).
double cjson_get_double(cjson_value*);

// Returns the intval of a value (must first check if it is integer).
int cjson_get_integer(cjson_value*);

// Sets the stringval and appropiate types/flags.
void cjson_set_string(cjson_value*, const char*);
// Sets the doubleval and appropiate flags for the value.
void cjson_set_double(cjson_value*, double);
// Sets the intval and appropiate flags for the value.
void cjson_set_integer(cjson_value*, int);

// Returns 1 if the value is true.
int cjson_true(cjson_value*);

// Returns 1 if the value is false.
int cjson_false(cjson_value*);

/*================ Object functions ================*/
// replaces the element at index idx with replacement. if old_value is not NULL then ownership of the replaced value is passed to the callee.
int cjson_replaceidx(cjson_value* p, int idx, cjson_value* replacement, cjson_value** old_value);
// erases the elment at index idx.
int cjson_eraseidx(cjson_value* p, int idx);
// appends element c to p. Ownership is passed onto p.
void cjson_append(cjson_value* p, cjson_value* c);
// returns 1 if the value passed to the function is an array.
int cjson_is_array(cjson_value*);
// returns the amount of elemnts inside the array.
int cjson_array_length(cjson_value*);
// returns the elment at index.
cjson_value* cjson_array_at(cjson_value*, int);

/*================ Object functions ================*/

// returns 1 if value was erased
int cjson_erase(cjson_value* p, const char* k);
// returns 1 if value was erased
int cjson_erasei(cjson_value* p, const char* k);
// returns 1 if value was replaced. If old_value != NULL then ownership is passed to callee and it is your responsibility to deallocate or use otherwise.
int cjson_replace(cjson_value* p, const char* k, cjson_value* replacement, cjson_value** old_value);
// inserts v into p, with the key k. Ownership is adopted by p.
void cjson_insert(cjson_value* p, const char* k, cjson_value* v);
// get the size of the object (the amount of keys it holds)
int cjson_object_size(cjson_value*);
// case-sensitive search for key k.
cjson_value* cjson_search_item(cjson_value* p, const char* k); // case-sensitive search
// case-insensitive search for key k.
cjson_value* cjson_searchi_item(cjson_value* p, const char* k); // case-insensitive search

// Check if array, object, or string is empty. Returns -1 in the case where the passed value is not of expected type or NULL.
int cjson_empty(cjson_value*);

// friendly string version of the current value type.
const char* cjson_type_string(cjson_value*);
// stringifies (serialize) the JSON value into JSON-formatted string. You must manually free the buffer if it is nonnull.
char* cjson_stringify(cjson_value*);

#define CJSON_OBJECT_FOR_EACH(object, k, v, body) cjson_value* iter_##object = object->child; \
    while (iter_##object != NULL) { \
        const char* k = iter_##object->string; \
        cjson_value* v = iter_##object->child; \
        body \
        iter_##object = iter_##object->next; \
    }

#define CJSON_ARRAY_FOR_EACH(array, v, body) cjson_value* iter_##array = array->child; \
    while (iter_##array != NULL) { \
        cjson_value* v = iter_##array; \
        body \
        iter_##array = iter_##array->next; \
    }

#endif
