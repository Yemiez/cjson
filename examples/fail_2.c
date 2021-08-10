#include "cjson/cjson.h"
#include <stdio.h>

int main()
{
	cjson_set_permissive(1);
    cjson_value* arr = cjson_parse("[1, 2, {\"key\": 123");
    if (!arr) {
        fprintf(stderr, "Failed to parse: %s\n", cjson_error_string());
        return -1;
    }
	char* buf = cjson_stringify(arr);
	printf("%s\n", buf);
	free(buf);

	cjson_free_value(arr);
	cjson_shutdown();
    return 0;
}
