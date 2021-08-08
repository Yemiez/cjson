#include "cjson/cjson.h"
#include <stdio.h>

int main()
{
    cjson_value* obj = cjson_parse("{ \"key\": 1, \"key2\": 2 }");
    if (!obj) {
        fprintf(stderr, "Could not parse: %s\n", cjson_error_string());
        return 1;
    }

	char* stringified = cjson_stringify(obj);
	printf("Before: %s\n", stringified);
	free(stringified);

    printf("Size before: %d\n", cjson_object_size(obj));
    cjson_erase(obj, "key");
    printf("Size after: %d\n", cjson_object_size(obj));

	stringified = cjson_stringify(obj);
	printf("Before: %s\n", stringified);
	free(stringified);


    cjson_free_value(obj);
    cjson_shutdown();
    return 0;
}
