/* RFC 4627 compliant JavaScript Object Notation (JSON) parser/generator.

Features a simple, efficient, secure and thread-safe interface for parsing and
generating volid JSON data.  Invalid JSON data is rejected by the parser.

All strings are represented in canonical UTF-8 encoding.  In particular, Unicode
escape sequences in string literals are converted to UTF-8 text.

Limitations:
    - UTF-16 surrogate pairs are not converted to single characters.
    - The parser requires stack space proportional to the maximum nesting depth
      of the structure being parsed; for this reason, the nesting depth is
      limited to 1,000 by default.
*/
#ifndef JSON_H_INCLUDED
#define JSON_H_INCLUDED

#include <stdlib.h>

typedef enum JsonType { JSON_NULL = 0, JSON_BOOLEAN, JSON_NUMBER,
                        JSON_STRING, JSON_ARRAY, JSON_OBJECT } JsonType;

typedef enum JsonBoolean { JSON_FALSE = 0, JSON_TRUE = 1 } JsonBoolean;

typedef double JsonNumber;  /* NOTE: only finite values are allowed! */

typedef struct JsonString   /* NOTE: might contain embedded zeroes! */
{
    size_t      size;       /* string size in bytes */
    char        *data;      /* zero-terminated UTF-8 data */
} JsonString;

typedef struct JsonValue        JsonValue;
typedef struct JsonNamedValue   JsonNamedValue;

typedef struct JsonArray
{
    size_t      size;
    JsonValue   *values;
} JsonArray;

typedef struct JsonObject
{
    size_t          size;
    JsonNamedValue  *values;
} JsonObject;

struct JsonValue
{
    JsonType type;
    union {
        JsonBoolean boolean;
        JsonNumber  number;
        JsonString  string;
        JsonArray   array;
        JsonObject  object;
    };
};

typedef struct JsonNamedValue
{
    JsonString  name;
    JsonValue   value;
} JsonNamedValue;


/* Parse a JSON string `input` and stores the result in `value`. */
JsonValue *json_parse(const char *input, JsonValue *value);
/*
On success, `value` is returned and must eventually be destroyed with
json_destroy_value().  On error, NULL is returned instead.

This function is a thin wrapper around json_parse_value() but also fails if:
    1. the parsed value is of a different type than Array or Object, or
    2. there is unparsed (non-whitespace) data at the end of the input buffer.
*/

/* Parses a JSON string pointed to by *ppos. */
JsonValue *json_parse_value( int max_depth,
                             JsonValue *result, const char **ppos );
/*
The recursion depth is limited to `max_depth` structures (Objects or Arrays).

If parsing succeeds, `result` is returned, indicating that `result` has been
initialized and must be destroyed using json_destroy_value(), and *ppos points
to the end of the parsed JSON data.

In case of error, NULL is returned, the contents of `result` are unchanged,
and *ppos is updated to the position of the first token that could not be
parsed.  There are three error conditions:

    1. The passed JSON data is invalid.
    2. Memory allocation failed.  In this case, errno == ENOMEM. (Note that
       the caller must initialize errno to zero beforehand to detect this!)
    3. Maximum nesting depth exceeded. (Note that there is no way to
       distinguish this failure from the first case!)

This function is useful to parse JSON values that are followed by other data
(whether it is JSON data or not), to get more information about the position
at which parsing failed, and to parse values of other types than JSON_ARRAY
and JSON_OBJECT (which are rejected by json_parse() as required by the JSON
specification).
*/

/* Frees all resources allocated for the given JSON value. */
void json_destroy_value(const JsonValue *value);
/*
Note that if `value` itself was heap-allocated, it is not freed by this
function, and the caller should call free(value) afterwards to do so.
*/

/* Utility function to skip JSON whitespace in a string. */
const char *json_skip_whitespace(const char *s);
/* Returns a pointer to the first non-whitespace character in `s`, or to the
   end of the string is `s` consists of whitespace only. */

/* Convert a JSON value to a compact string representation. */
char *json_format(const JsonValue *value);
/*
The result is a dynamically allocated zero-terminated string that must be freed
by the the caller.  In case of a memory allocation failure, NULL is returned.
*/

/* Convert a JSON value to a compact string representation. */
size_t json_format_buffer(const JsonValue *value, char *out, size_t len);
/*
The result is written to `out`.  No more than `len` characters are written, and
the final buffer will be zero-terminated (unless `len` is zero, in which case
no output is written at all.)

This function always returns the total string length required to store the
result (regardless of the size of the output buffer) excluding the terminating
zero character.  If this length is greater than or equal to `len` then the
output has been truncated.  Note that this matches the semantics of snprintf().
*/

#endif /* ndef JSON_H_INCLUDED */
