#include "cjson/cjson.h"
#include <stdio.h>

int main()
{
    cjson_value* arr = cjson_parse("[1, 2,");
    if (!arr) {
        fprintf(stderr, "Failed to parse: %s\n", cjson_error_string());
		cjson_shutdown();
        return -1;
    }

	cjson_free_value(arr);
	cjson_shutdown();
    return 0;
}
