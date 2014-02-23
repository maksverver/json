#include "json.h"
#include <stdio.h>

static char buf[1<<20];                 /* max 1 MB of input */

int main()
{
    JsonValue value;
    buf[fread(buf, 1, sizeof(buf) - 1, stdin)] = '\0';
    if (json_parse((const char*)buf, &value) == NULL)
    {
        const char *pos = buf;
        if (json_parse_value(JSON_MAX_NESTING_DEPTH, &value, &pos) == NULL)
        {
            printf("Parsing failed at byte %d: '%.5s'!\n", (int)(pos - buf), pos);
            value.type = JSON_NULL;
        }
        else if (value.type != JSON_ARRAY && value.type != JSON_OBJECT)
        {
            printf("JSON value is not an array or object (type: %d)\n", value.type);
        }
        else if (*(pos = json_skip_whitespace(pos)) != '\0')
        {
            printf("Extra data after input at byte %d: '%.5s'!\n", (int)(pos - buf), pos);
        }
        else
        {   /* should not be possible to get here: */
            printf("Unknown parse error occurred!\n");
        }
        json_destroy_value(&value);
    }
    else
    {
        char *out = json_format(&value);
        json_destroy_value(&value);
        printf("<<%s>>\n", out);
        free(out);
    }
    return 0;
}
