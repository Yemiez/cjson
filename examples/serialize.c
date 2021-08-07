#include "cjson/cjson.h"
#include <stdio.h>

int main()
{
    cjson_value* object = cjson_create_object();
    if (!object) {
        fprintf(stderr, "Failed to create empty object!\n");
        return -1;
    }

    // Insert the values
    cjson_insert(object, "pi", cjson_create_double(3.14));
    cjson_insert(object, "happy", cjson_create_boolean(1));
    cjson_insert(object, "name", cjson_create_string("Oskar"));

    // Serialize
    char* serialized = cjson_stringify(object);
    if (!serialized) {
        fprintf(stderr, "Failed serialize object!\n");
        return -1;
    }

    printf("Serialized: %s\n", serialized);
    free(serialized); // Remember to always free the buffer returned from cjson_stringify

    // Only the object needs to be freed because it is the parent of all the other values we created.
    cjson_free_value(object); 
    return 0;
}