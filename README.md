# CJSON - JSON Parser in C
A simple JSON parser made in C, compiled and tested on Linux (Ubuntu 20.04, GCC 10.2.0) and Windows (GCC 10.2.0).

This is a sample project, used as an education tool to become more proficient in C

## Building
The library only consists of two files: `cjson.h` and its counterpart `cjson.c`. To compile it you can just pass this to your preferred compiler. You can also use the script files `build-linux.sh` or `build-win32.sh` (this will build the example executable with `main.c`).

## Examples
There are some examples inside of the `examples/` directory.

### Parsing
To parse a file you can simply call:
```c
cjson_value* parsed = cjson_parse_file("filename.json");
```

And to parse a string literal:
```c
cjson_value* parsed = cjson_parse("[1, 2, 3]");
```

### Error handling
If any of the `cjson_parse` variants fail they will return a NULL value. 
You can simply retrieve the error code and error string with the following:
```c
int errc = cjson_error_code(); // The error code, see enum `cson_error_code_type` for possible values.
const char* errmsg = cjson_error_string(); // A friendly description of the error code.
```

### Object functions
You can loop an object with the help of the `CJSON_OBJECT_FOR_EACH` macro. Example usage:
```c
CJSON_OBJECT_FOR_EACH(object, key, value, {
	printf("My key is '%s' and my value type is '%s'\n", key, cjson_type_string(value));
});
```

Inserting a key-value pair:
```c
cjson_insert(object, "the key", the_value);
// Note that 'the_value' is now owned by 'object'. 
```

Erasing a key from an object:
```c
// did_erase will be 1 if the key existed and was erased.
int did_erase = cjson_erase(object, "the key");

// of course there is also a case-insensitive version 
int did_erase = cjson_erasei(object, "the key");
```

Replacing a value in an object:
```c
cjson_value* new_value = cjson_create_integer(1337);
cjson_value* old_value;

// Try to replace
int did_replace = cjson_replace(object, "the key", new_value, &old_value);
if (did_replace) {
	// Now we must either insert old_value somewhere else, or free it ourselves.
	cjson_free_value(old_value);
}

// You can, of course, also let the replace function free the old value by passing NULL as the out ptr for old value.
int did_replace = cjson_replace(object, "the key", new_value, NULL);

// Now we can be sure that if the item was replaced, it was also properly freed.
```
> Note that the `cjson_replace` function does not have a case-insensitive version.

You can search for an item inside of an object like this:
```c
cjson_value* pi = cjson_search_item(object, "pi");
// If the key 'pi' doesn't exist, then the variable pi will be NULL.

cjson_insert(object, "nAmE", cjson_create_string("Oskar"));

// You can also do a case-insensitive search:
cjson_value* name_fail = cjson_search_item(object "name"); // name_fail = NULL
cjson_value* name = cjson_searchi_item(object "name"); // name = ptr to the value.
```

### Looping an array
Looping an array is done similarly to an object with the help the `CJSON_ARRAY_FOR_EACH` macro. Usage:
```c
CJSON_ARRAY_FOR_EACH(array, value, {
	printf("Element value type is '%s'\n", cjson_type_string(value));
});
```

### Serializing
To serialize any JSON value you can use the function `cjson_stringify`. This is capapble of serializing every possible type that a `cjson_value` can be. Usage:
```c
char* buf = cjson_stringify(my_cjson_value); // Buf is a null-terminated string containing the JSON value in its written form.

// Do stuff with buf

free(buf); // IMPORTANT! The buffer returned from stringify must always be manually freed with free().
```
> Note that `cjson_stringify` may return `NULL` upon failure, you should always check this before attempting to use or free the buffer.

### Creating JSON values manually
You can create JSON values using the utility `cjson_create_xxx` functions. Example:
```c
cjson_value* object = cjson_create_object();
if (!object) {
	fprintf(stderr, "Failed to create empty object!\n");
	return -1;
}

// Insert the values (object adopts ownership of the created values)
cjson_insert(object, "pi", cjson_create_double(3.14));
cjson_insert(object, "happy", cjson_create_boolean(1));
cjson_insert(object, "name", cjson_create_string("Oskar"));

// We can then serialize it as with any other cjson_value.
char* buf = cjson_stringify(object); // buf = {"pi":3.14,"happy":true,"name":"Oskar"}
printf("Serialized: %s\n", buf);
free(buf);

// Now we only need to free the top-level parent (object).
// It will then in turn free all of it's children.
cjson_free_value(object);
```

You can of course also do this with arrays:
```c
cjson_value* arr = cjson_create_array();
if (!arr) {
	fprintf(stderr, "Failed to create empty array!\n");
	return -1;
}

// append the elements
cjson_append(arr, cjson_create_integer(1));
cjson_append(arr, cjson_create_integer(2));
cjson_append(arr, cjson_create_integer(3));
cjson_append(arr, cjson_create_integer(4));

// We can then serialize it as with any other cjson_value.
char* buf = cjson_stringify(arr); // buf = [1,2,3,4]
printf("Serialized: %s\n", buf);
free(buf);

// And as with objects, we only need to free the top-level parent.
cjson_free_value(arr);
```

## TODO
* Documentation and examples
* Utility functions (such as convenient lookup functions, i.e. `key>depth1>depth2>depth3>[4]`);.
* Optimise memory usage (currently the internal `cjson_state*` hogs up a lot of memory during parse);
* Optimise general speed of parsing (replace function calls?).
* Figure out if everything I've done is good or bad C :^)
* Add proper multi-threading support when memory-logging is enabled.
* Fix some memory leaks that happen when parsing fails (in specific `cjson.c!cjson_parse_impl`)

