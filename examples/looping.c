#include "cjson/cjson.h"
#include <stdio.h>

int main()
{
    cjson_value* arr = cjson_parse("[1, 2, 3, 4, 5, 6]");
    if (!arr) {
        fprintf(stderr, "Failed to parse: %s\n", cjson_error_string());
        return -1;
    }

    CJSON_ARRAY_FOR_EACH(arr, elem, {
        char* str = cjson_stringify(elem);
        if (!str) {
            fprintf(stderr, "Failed to stringify elem!\n");
            continue;
        } 
        printf("%6s\n", str);
        free(str); // Free it as expected
    })

    return 0;
}