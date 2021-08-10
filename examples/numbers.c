#include "cjson/cjson.h"
#include <stdio.h>

int main()
{	
	cjson_value* arr = cjson_parse("[1, 2, 3, 4, 5, -10, 20.489, 3.14159265359, -3.14]");
	if (!arr) {
		fprintf(stderr, "Failed to parse: %s\n", cjson_error_string());
		return 1;
	}
	printf("Numbers:\n");
	printf("\t");
	CJSON_ARRAY_FOR_EACH(arr, elem, {
		if (cjson_is_integer(elem)) {
			printf("i=%d", cjson_get_integer(elem));
		}
		else {
			printf("d=%g", cjson_get_double(elem));
		}

		if (elem->next) {
			printf(", ");
		}
	});
	printf("\n");

	cjson_free_value(arr);
	cjson_shutdown();
	return 0;
}
