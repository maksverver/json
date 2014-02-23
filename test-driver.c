#include "json.h"
#include <stdio.h>
#include <string.h>

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
        if (out == NULL)
        {
            printf("Internal error: failed to generate JSON text!\n");
        }
        else
        {
            printf("<<%s>>\n", out);

            JsonValue value2;
            if (json_parse(out, &value2) == NULL)
            {
                printf("Internal error: failed to parse generated JSON text!\n");
            }
            else
            {
                char *out2 = json_format(&value2);
                if (out2 == NULL)
                {
                    printf("Internal error: failed to regenerate JSON text!\n");
                }
                else
                {
                    if (strcmp(out, out2) != 0)
                    {
                        printf("Internal error: regenerated JSON text differs!\n");
                    }
                    free(out2);
                }
                json_destroy_value(&value2);
            }
        }
        json_destroy_value(&value);
    }
    return 0;
}
