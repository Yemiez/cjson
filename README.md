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

### Looping an object
You can loop an object with the help of the `CJSON_OBJECT_FOR_EACH` macro. Example usage:
```c
CJSON_OBJECT_FOR_EACH(object, key, value, {
	printf("My key is '%s' and my value type is '%s'\n", key, cjson_type_string(value));
});
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

## TODO
* Documentation and examples
* Utility functions (such as convenient lookup functions, i.e. `key>depth1>depth2>depth3>[4]`);.
* Optimise memory usage (currently the internal `cjson_state*` hogs up a lot of memory during parse);
* Optimise general speed of parsing (replace function calls?).
* Figure out if everything I've done is good or bad C :^)
* Add proper multi-threading support when memory-logging is enabled.
* Fix some memory leaks that happen when parsing fails (in specific `cjson.c!cjson_parse_impl`)

