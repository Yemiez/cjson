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
} cjson_settings;

typedef enum {
    cjson_invalid, // invalid (internal only, should never be returned from cjson_parse variants)
    cjson_kv, // key-value (for objects, string = key, child = value)
    cjson_string, 
    cjson_number, 
    cjson_object, 
    cjson_array, 
    cjson_boolean, 
    cjson_null
} cjson_type;

typedef enum {
    cjson_flag_double = 1 << 1,
    cjson_flag_integer = 1 << 2,
} cjson_flags;

typedef struct {
    size_t row;
    size_t col;
    size_t ofs;
} cjson_pos;

// You should never directly access the fields inside here.
typedef struct __cjson_value {
    struct __cjson_value* prev;
    struct __cjson_value* next;
    struct __cjson_value* child;
    struct __cjson_value* childtail; // cached value.
    cjson_type type;
    char* string;
    double doubleval;
    int intval;
    int flags;
} cjson_value;

// Pass NULL for default
void cjson_init(cjson_settings*);
void cjson_print_mem(void);
void cjson_shutdown(void); 

cjson_value* cjson_parse_file(const char* filename);
cjson_value* cjson_parse(const char* buffer);
void cjson_free_value(cjson_value*);
int cjson_error_code(void);
const char* cjson_error_string(void);

// Creating custom objects
cjson_value* cjson_create_object(); // create empty object
cjson_value* cjson_create_array(); // create empty array
cjson_value* cjson_create_null(); // create a cjson_null value
cjson_value* cjson_create_boolean(int value); // create a boolean value
cjson_value* cjson_create_int(int value); // create a number that holds an integer
cjson_value* cjson_create_double(double value); // create a number that holds a double
cjson_value* cjson_create_string(const char* string); // create a string

// Check & set functions
int cjson_is_string(cjson_value*);
int cjson_is_number(cjson_value*);
int cjson_is_double(cjson_value*);
int cjson_is_integer(cjson_value*);
int cjson_is_object(cjson_value*);
int cjson_is_boolean(cjson_value*);
int cjson_is_null(cjson_value*);

const char* cjson_get_string(cjson_value*);
double cjson_get_double(cjson_value*);
int cjson_get_integer(cjson_value*);

void cjson_set_string(cjson_value*, const char*);
void cjson_set_double(cjson_value*, double);
void cjson_set_integer(cjson_value*, int);

// Handy functions for boolean values
int cjson_true(cjson_value*);
int cjson_false(cjson_value*);

// // Array functions
void cjson_append(cjson_value* p, cjson_value* c); // alias for push_child
void cjson_push_child(cjson_value* p, cjson_value* c);
int cjson_is_array(cjson_value*);
int cjson_array_length(cjson_value*);
cjson_value* cjson_array_at(cjson_value*, int);

// Object functions
void cjson_insert(cjson_value* p, const char* k, cjson_value* v); // alias for push_item
void cjson_push_item(cjson_value* p, const char* k, cjson_value* v);
int cjson_object_size(cjson_value*);
cjson_value* cjson_search_item(cjson_value* p, const char* k); // case-sensitive search
cjson_value* cjson_searchi_item(cjson_value* p, const char* k); // case-insensitive search

// Check if array, object, or string is empty. Returns -1 in the case where the passed value is not of expected type or NULL.
int cjson_empty(cjson_value*);

// friendly string version of the current value type.
const char* cjson_type_string(cjson_value*);

// You must free the buffer with free() (it is a regular malloc'd string)
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