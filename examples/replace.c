#include "cjson/cjson.h"
#include <stdio.h>

int main()
{  
    cjson_value* obj = cjson_parse("{ \"key\": 1 }");
    if (!obj) {
        fprintf(stderr, "Could not parse: %s\n", cjson_error_string());
        return 1;
    }

    printf("Old value: %d\n", cjson_get_integer(cjson_search_item(obj, "key")));
    if (!cjson_replace(obj, "key", cjson_create_string("Injected!"), NULL)) {
        fprintf(stderr, "Failed to replace!\n");
        return 1;
    }

    printf("New value: %s\n", cjson_get_string(cjson_search_item(obj, "key")));
    cjson_free_value(obj);
    return 0;
}