#include "cjson/cjson.h"
#include <stdio.h>

int main()
{
    cjson_value* obj = cjson_parse("{ \"pi\": 3.14, \"happy\": true, \"name\": \"Oskar\" }");
    if (!obj) {
        fprintf(stderr, "Failed to parse: %s\n", cjson_error_string());
        return -1;
    }

    CJSON_OBJECT_FOR_EACH(obj, k, v, {
        printf("Object has key: %s\n", k);
        v;
    })

    cjson_value* pi = cjson_search_item(obj, "pi");
    cjson_value* happy = cjson_search_item(obj, "happy");
    cjson_value* name = cjson_search_item(obj, "name");

    if (!pi) {
        fprintf(stderr, "Searching for key 'pi' failed");
        return -1;
    }
    if (!happy) {
        fprintf(stderr, "Searching for key 'happy' failed");
        return -1;
    }
    if (!name) {
        fprintf(stderr, "Searching for key 'name' failed");
        return -1;
    }

    printf("Pi is: %g\n", cjson_get_double(pi));
    printf("Name is: %s\n", cjson_get_string(name));
    printf("and he is: %s\n", cjson_true(happy) ? "happy" : "sad");
    return 0;
}