#include "cjson/cjson.h"
#include <stdio.h>

int main()
{
    cjson_init(NULL);

    /*
    cjson_value* value = cjson_parse(
        "{"
            "\"key1\"" ":" "[1, 2, 3, 4]" ","
            "\"key2\"" ":" "null" ","
            "\"key3\"" ":" "true" ","
            "\"key4\"" ":" "false" ","
            "\"key5\"" ":" "{\"depth\": 2}" ","
        "}"
    );*/
   cjson_value* value = cjson_parse_file("tests/jeopardy_questions.json");

    if (value) {
        printf("seemingly successful parse, type is %s, is %d long\n", cjson_type_string(value), cjson_array_length(value));

        int count = 0;
        CJSON_ARRAY_FOR_EACH(value, v, {
            if (count == 100) {
                break;
            }

            printf("%s\n", cjson_stringify(v));
            ++count;
        });
    }
    else {
        printf("%s\n", cjson_error_string());
    }

    cjson_free_value(value);
    cjson_print_mem();
    cjson_shutdown();
    return 0;
}