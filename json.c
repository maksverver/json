#include <stdio.h>
#include <string.h>
#include "json.h"

#ifndef JSON_MAX_NESTING_DEPTH
#define JSON_MAX_NESTING_DEPTH 1000
#endif

void json_destroy_value(const JsonValue *value)
{
    size_t i;
    switch (value->type)
    {
    case JSON_NULL:
    case JSON_BOOLEAN:
    case JSON_NUMBER:
        break;

    case JSON_STRING:
        free(value->string.data);
        break;

    case JSON_ARRAY:
        for (i = 0; i < value->array.size; ++i)
        {
            json_destroy_value(&value->array.values[i]);
        }
        free(value->array.values);
        break;

    case JSON_OBJECT:
        for (i = 0; i < value->object.size; ++i)
        {
            free(value->object.values[i].name.data);
            json_destroy_value(&value->object.values[i].value);
        }
        free(value->object.values);
        break;
    }
}

static int is_space(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

static int parse_hexdigit(char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'z') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'Z') return ch - 'A' + 10;
    return -1;
}

const char *json_skip_whitespace(const char *s)
{
    while (*s && is_space(*s)) ++s;
    return s;
}

/* Shrinks a dynamic memory object to the requested `size` similar to realloc(),
   but unlike realloc() this returns the original pointer if allocation fails. */
static void *shrink(void *ptr, size_t size)
{
    void *res = realloc(ptr, size);
    return res != NULL ? res : ptr;
}

/* Decodes a JSON string token (enclosed in double quotes) by translating escape
   sequences into UTF-8 sequences.  Assumes the token is formatted correctly.
   Returns a dynamically allocated string, or NULL if allocation failed. */
static char *decode_string(const char *begin, const char *end, size_t *size)
{
    const char *p;
    char *buf = malloc(end - begin - 1), *q = buf;

    if (buf == NULL) return NULL;

    for (p = begin + 1; p < end - 1; ++p)
    {
        if (*p != '\\')
        {
            *q++ = *p;
        }
        else switch (*++p)
        {
        default:  *q++ = *p;   break;  /* one of '/', '\' or '"' */
        case 'b': *q++ = '\b'; break;
        case 'f': *q++ = '\f'; break;
        case 'n': *q++ = '\n'; break;
        case 'r': *q++ = '\r'; break;
        case 't': *q++ = '\t'; break;
        case 'u': 
            {   /* decode character encoded with four hex digits: */
                unsigned u  = (unsigned)parse_hexdigit(p[1]) << 12
                            | (unsigned)parse_hexdigit(p[2]) <<  8
                            | (unsigned)parse_hexdigit(p[3]) <<  4
                            | (unsigned)parse_hexdigit(p[4]) <<  0;
                if (u < 0x80)  /* ASCII character */
                {
                    *q++ = u;
                }
                else if (u < 0x800)  /* 2-byte UTF-8 sequence */
                {
                    *q++ = 0xc0 | ((u >> 6)      );
                    *q++ = 0x80 | ((u     ) &0x3f);
                }
                else  /* 3-byte UTF-8 sequence */
                {
                    *q++ = 0xe0 | ((u >> 12)     );
                    *q++ = 0x80 | ((u >>  6)&0x3f);
                    *q++ = 0x80 | ((u      )&0x3f);
                }
                p += 4;
            }
        }
    }
    *q = '\0';  /* zero-terminate buffer */
    *size = q - buf;
    return shrink(buf, *size + 1);
}

static JsonValue *parse_string(JsonValue *result, const char **ppos)
{
    char *data;
    size_t size;
    const char *pos = *ppos + 1;

    while (*pos != '"')
    {
        if ((unsigned)*pos < 32) return NULL;
        if (*pos++ == '\\')
        {
            if (strchr("\"\\/bfnrtu", *pos) == NULL) return NULL;
            if (*pos++ == 'u')
            {
                if ( (unsigned)parse_hexdigit(pos[0]) >= 16 ||
                     (unsigned)parse_hexdigit(pos[1]) >= 16 ||
                     (unsigned)parse_hexdigit(pos[2]) >= 16 ||
                     (unsigned)parse_hexdigit(pos[3]) >= 16 )
                {
                    return NULL;
                }
                pos += 4;
            }
        }
    }
    data = decode_string(*ppos, pos + 1, &size);
    if (data == NULL) return NULL;
    result->type = JSON_STRING;
    result->string.data = data;
    result->string.size = size;
    *ppos = pos + 1;
    return result;
}

static JsonValue *parse_number(JsonValue *result, const char **ppos)
{
    const char *pos = *ppos;
    if (*pos == '-') ++pos;  /* optional leading sign */
    if (*pos == '0') ++pos;
    else if (*pos >= '1' && *pos <= '9')
    {
        do ++pos; while (*pos >= '0' && *pos <= '9');
    }
    else return NULL;
    if (*pos == '.')  /* optional fractional part */
    {
        ++pos;
        if (*pos >= '0' && *pos <= '9')
        {
            do ++pos; while (*pos >= '0' && *pos <= '9');
        }
        else return NULL;
    }
    if (*pos == 'E' || *pos == 'e')  /* optional exponent */
    {
        ++pos;
        if (*pos == '+' || *pos == '-') ++pos;
        if (*pos >= '0' && *pos <= '9')
        {
            do ++pos; while (*pos >= '0' && *pos <= '9');
        }
        else return NULL;
    }
    result->type   = JSON_NUMBER;
    result->number = atof(*ppos);   /* FIXME: atof() is locale-dependent! */
    *ppos = pos;
    return result;
}

static JsonValue *parse_array(int depth, JsonValue *result, const char **ppos)
{
    size_t size = 0, capacity = 0;
    JsonValue *values = NULL;

    for (++*ppos; *(*ppos = json_skip_whitespace(*ppos)) != ']'; ++size)
    {
        if (size > 0)
        {
            if (**ppos == ',') ++*ppos; else goto failed;
        }
        if (size == capacity)
        {
            capacity = capacity > 0 ? 2*capacity : 4;
            JsonValue *a = realloc(values, capacity*sizeof(JsonValue));
            if (a == NULL) goto failed;
            values = a;
        }
        if (json_parse_value(depth, &values[size], ppos) == NULL) goto failed;
    }
    ++*ppos;
    result->type = JSON_ARRAY;
    result->array.size   = size;
    result->array.values = shrink(values, size*sizeof(JsonValue));
    return result;

failed:
    while (size-- > 0) json_destroy_value(&values[size]);
    free(values);
    return NULL;
}

static JsonValue *parse_object(int depth, JsonValue *result, const char **ppos)
{
    size_t size = 0, capacity = 0;
    JsonNamedValue *nvalues = NULL;
    JsonValue name = { JSON_NULL };

    for (++*ppos; *(*ppos = json_skip_whitespace(*ppos)) != '}'; ++size)
    {
        if (size > 0)
        {
            if (**ppos != ',') goto failed;
            *ppos = json_skip_whitespace(*ppos + 1);
        }
        if (**ppos != '"' || parse_string(&name, ppos) == NULL) goto failed;
        if (size == capacity)
        {
            capacity = capacity > 0 ? 2*capacity : 2;
            JsonNamedValue *a = realloc(nvalues, capacity*sizeof(JsonNamedValue));
            if (a == NULL) goto failed;
            nvalues = a;
        }
        *ppos = json_skip_whitespace(*ppos);
        if (**ppos != ':') goto failed;
        ++*ppos;
        if (json_parse_value(depth, &nvalues[size].value, ppos) == NULL) goto failed;
        nvalues[size].name = name.string;
        name.type = JSON_NULL;
    }
    ++*ppos;
    result->type = JSON_OBJECT;
    result->object.size   = size;
    result->object.values = shrink(nvalues, size*sizeof(JsonNamedValue));
    return result;

failed:
    json_destroy_value(&name);
    while (size-- > 0)
    {
        free(nvalues[size].name.data);
        json_destroy_value(&nvalues[size].value);
    }
    free(nvalues);
    return NULL;
}

JsonValue *json_parse_value(int depth, JsonValue *result, const char **ppos)
{
    switch (*(*ppos = json_skip_whitespace(*ppos)))
    {
    case '[':
        return depth > 0 ? parse_array(depth - 1, result, ppos) : NULL;

    case '{':
        return depth > 0 ? parse_object(depth - 1, result, ppos) : NULL;

    case 'n':
        if (strncmp(*ppos, "null", 4) != 0) return NULL;
        *ppos += 4;
        result->type = JSON_NULL;
        return result;

    case 'f':
        if (strncmp(*ppos, "false", 5) != 0) return NULL;
        *ppos += 5;
        result->type    = JSON_BOOLEAN;
        result->boolean = JSON_FALSE;
        return result;

    case 't':
        if (strncmp(*ppos, "true", 4) != 0) return NULL;
        *ppos += 4;
        result->type    = JSON_BOOLEAN;
        result->boolean = JSON_TRUE;
        return result;

    case '"':
        return parse_string(result, ppos);

    default:
        return parse_number(result, ppos);
    }
}

JsonValue *json_parse(const char *input, JsonValue *result)
{
    JsonValue value = { JSON_NULL };
    if ( json_parse_value(JSON_MAX_NESTING_DEPTH, &value, &input) != NULL &&
         (value.type == JSON_ARRAY || value.type == JSON_OBJECT) &&
         (*json_skip_whitespace(input) == '\0') )
    {
        *result = value;
        return result;
    }
    else
    {
        json_destroy_value(&value);
        return NULL;
    }
}

static const char *char_escape(char ch)
{
    static const char *escapes[32] = {
        "\\u0000", "\\u0001", "\\u0002", "\\u0003",
        "\\u0004", "\\u0005", "\\u0006", "\\u0007",
        "\\b",     "\\t",     "\\n",     "\\u000b",
        "\\f",     "\\r",     "\\u000e", "\\u000f",
        "\\u0010", "\\u0011", "\\u0012", "\\u0013",
        "\\u0014", "\\u0015", "\\u0016", "\\u0017",
        "\\u0018", "\\u0019", "\\u001a", "\\u001b",
        "\\u001c", "\\u001d", "\\u001e", "\\u001f" };
    if (ch == '"') return "\\\"";
    if (ch == '\\') return "\\\\";
    return ((unsigned)ch < 32) ? escapes[(unsigned)ch] : NULL;
}

size_t format_string(const JsonString *string, char *out, size_t len)
{
    size_t i, n = 0;
    if (n < len) out[n] = '"';
    ++n;
    for (i = 0; i < string->size; ++i)
    {
        const char *s = char_escape(string->data[i]);
        if (s == NULL)
        {
            if (n < len) out[n] = string->data[i];
            ++n;
        }
        else
        {
            n += snprintf( n < len ? out + n : NULL,
                           n < len ? len - n : 0, "%s", s );
        }
    }
    if (n < len) out[n] = '"';
    ++n;
    return n;
}

/* Returns the decimal precision required to exactly represent the given number,
   with a minimum of 6 (so small integers are represented naturally). */
static int get_precision(double x)
{
    int lo = 6, hi = 20;
    while (lo < hi)
    {
        char buf[30];
        int mid = (lo + hi) >> 1;
        sprintf(buf, "%.*g", mid, x);
        if (atof(buf) != x) lo = mid + 1; else hi = mid;
    }
    return lo;
}

static size_t format(const JsonValue *value, char *out, size_t len)
{
    switch (value->type)
    {
    case JSON_NULL:
        return snprintf(out, len, "%s", "null");

    case JSON_BOOLEAN:
        return snprintf(out, len,  "%s", value->boolean ? "true" : "false");

    case JSON_STRING:
        return format_string(&value->string, out, len);

    case JSON_NUMBER:
        return snprintf( out, len, "%.*g",
                         get_precision(value->number), value->number );

    case JSON_ARRAY:
        {
            size_t i, n = 0;
            if (n < len) out[n] = '[';
            ++n;
            for (i = 0; i < value->array.size; ++i)
            {
                if (i > 0)
                {
                    if (n < len) out[n] = ',';
                    ++n;
                }
                n += format( &value->array.values[i],
                             n < len ? out + n : NULL,
                             n < len ? len - n : 0 );
            }
            if (n < len) out[n] = ']';
            ++n;
            return n;
        }

    case JSON_OBJECT:
        {
            size_t i, n = 0;
            if (n < len) out[n] = '{';
            ++n;
            for (i = 0; i < value->array.size; ++i)
            {
                if (i > 0)
                {
                    if (n < len) out[n] = ',';
                    ++n;
                }
                n += format_string( &value->object.values[i].name,
                                    n < len ? out + n : NULL,
                                    n < len ? len - n : 0 );
                if (n < len) out[n] = ':';
                ++n;
                n += format( &value->object.values[i].value,
                             n < len ? out + n : NULL,
                             n < len ? len - n : 0 );
            }
            if (n < len) out[n] = '}';
            ++n;
            return n;
        }
    }
    abort();  /* should be unreachable */
    return 0;
}

size_t json_format_buffer(const JsonValue *value, char *out, size_t len)
{
    size_t n = format(value, out, len);
    if (len > 0) out[n < len ? n : len - 1] = '\0';  /* zero-terminate `out` */
    return n;
}

char *json_format(const JsonValue *value)
{
    size_t n = format(value, NULL, 0);
    char *s = malloc(n + 1);
    format(value, s, n + 1);
    s[n] = '\0';
    return s;
}
