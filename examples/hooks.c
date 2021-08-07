#include "cjson/cjson.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int object_start(cjson_value* parent, cjson_value* object)
{
    printf("enter object\n");
    return 1;
}
int object_key_value(cjson_value* object, const char* key, cjson_value* the_value)
{
    printf("key='%s' value='%s'\n", key, cjson_type_string(the_value));
    return 1;
}

int object_end(cjson_value* object)
{
    printf("leave object\n");
    return 1;
}

// Array hooks
int array_start(cjson_value* parent, cjson_value* array)
{
    printf("enter array\n");
    return 1;
}

int array_element(cjson_value* array, cjson_value* element)
{
    printf("array element\n");
    return 1;
}

int array_end(cjson_value* array)
{
    printf("leave array\n");
    return 1;
}

int main()
{
    cjson_settings settings;
    memset(&settings, 0, sizeof(cjson_settings));
    settings.mem_alloc = &malloc;
    settings.mem_free = &free;

    settings.object_start = &object_start;
    settings.object_key_value = &object_key_value;
    settings.object_end = &object_end;

    settings.array_start = &array_start;
    settings.array_element = &array_element;
    settings.array_end = &array_end;

    cjson_init(&settings);
    
    cjson_value* json = cjson_parse_file("tests/test1.json");

    cjson_free_value(json);
    cjson_shutdown();
}