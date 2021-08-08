#include "cjson/cjson.h"
#include <stdio.h>

int main()
{	
	cjson_value* arr = cjson_parse("[1, 2, 3, 4, 5]");
	if (!arr) {
		fprintf(stderr, "Failed to parse: %s\n", cjson_error_string());
		return 1;
	}
	
	char* stringified = cjson_stringify(arr);
	printf("Stringified: %s\n", stringified);
	free(stringified);

	printf("Before length: %d\n", cjson_array_length(arr));
	int did_replace = cjson_replaceidx(arr, 0, cjson_create_string("1"), NULL);
	printf("Did erase: %d\n", did_replace);
	printf("Before length: %d\n", cjson_array_length(arr));
	
	stringified = cjson_stringify(arr);
	printf("Stringified: %s\n", stringified);
	free(stringified);

	cjson_free_value(arr);
	cjson_shutdown();
	return 0;
}
