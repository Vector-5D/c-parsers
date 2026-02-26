#include "json_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>

void json_skip_spaces(const char** text) {
    while (**text == ' ' || **text == '\n' || **text == '\t') {
        (*text)++;
    }
}

// currently exits on first double-quote
// this doesn't handle valid JSON like:
// 1. escaped quotes ("John \"Doe\"")
// 2. other escape sequences (\n, \t, \\, \uXXXX)
// it has no overflow protection if the string is extremely large.
// it also does not validate JSON spec

// returns NULL on error
char* json_read_string(const char** text) {
    if (**text != '"') return NULL;
    (*text)++; // skip first quote

    const char* start = *text;
    const char* scan_pos = start;

    // loop for finding the terminating quote
    while (*scan_pos) {
        if (*scan_pos == '"') {
            // get backslash count
            size_t backslash_count = 0;
            const char* previous = scan_pos - 1;
            
            while (previous >= start && *previous == '\\') {
                backslash_count++;
                previous--;
            }

            if (backslash_count % 2 == 0) {
                // even number of backslashes means its not escaped
                break;
            }
        }
        scan_pos++;
    }

    if (*scan_pos != '"') return NULL; // string should end on another double-quote
    
    size_t length = (size_t)(scan_pos - start);
    char* result = (char*)malloc(length + 1);
    if (!result) return NULL;

    const char* in = *text;
    char* out = result;

    while (in < scan_pos) {
        if (*in == '\\') {
            in++;
            if (in >= scan_pos) goto error;

            switch (*in) {
                case '"':
                    *out++ = '"';
                    break;
                case '\\':
                    *out++ = '\\';
                    break;
                case '/':
                    *out++ = '/';
                    break;
                case 'b':
                    *out++ = '\b';
                    break;
                case 'f':
                    *out++ = '\f';
                    break;
                case 'n':
                    *out++ = '\n';
                    break;
                case 'r':
                    *out++ = '\r';
                    break;
                case 't':
                    *out++ = '\t';
                    break;
                case 'u':
                    // TO-DO: implement the unicode code point escape character \uXXXX
                    goto error;
                default: goto error;
            }
        } else {
            // raw control characters are not allowed in JSON spec
            // 0x20 is the ASCII threshold for control characters
            if ((unsigned char)*in < 0x20) goto error;
            *out++ = *in;
        }
        in++;
    }
    
    *out = '\0';
    *text = scan_pos + 1; // skip ending double-quote
    return result;
    
    error:
        free(result);
        return NULL;
}

double json_read_number(const char** text) {
    char* end;
    double num = strtod(*text, &end);
    *text = end;
    return num;
}

int json_read_bool(const char** text) {
    if (strncmp(*text, "true", 4) == 0) {
        *text += 4;
        return 1;
    } else if (strncmp(*text, "false", 5) == 0) {
        *text += 5;
        return 0;
    }
    return 0;
}

json_value_t* json_read_value(const char** text); // forward declaration
void json_free_value(json_value_t* value); // forward declaration

// returns NULL on allocation failure
json_value_t* json_read_object(const char** text) {
    json_value_t* obj = (json_value_t*)malloc(sizeof(json_value_t));
    if (!obj) return NULL;
    
    obj->type = TYPE_OBJECT;
    obj->object_value.capacity = 8;
    obj->object_value.count = 0;
    obj->object_value.pairs = malloc(sizeof(json_pair_t) * obj->object_value.capacity);

    if (!obj->object_value.pairs) goto error;

    (*text)++; // skip '{'
    json_skip_spaces(text);
    while (**text != '}' && **text != '\0') {
        json_skip_spaces(text);
        char* key = json_read_string(text);
        if (!key) goto error;
        
        json_skip_spaces(text);
        if (**text == ':') (*text)++;
        json_skip_spaces(text);
        json_value_t* value = json_read_value(text);
        if (!value) goto error;
        
        if (obj->object_value.count == obj->object_value.capacity) {
            obj->object_value.capacity *= 2;
            json_pair_t* new_pairs = realloc(
                obj->object_value.pairs,
                sizeof(json_pair_t) * obj->object_value.capacity
            );

            if (!new_pairs) goto error;

            obj->object_value.pairs = new_pairs;
        }

        obj->object_value.pairs[obj->object_value.count].key = key;
        obj->object_value.pairs[obj->object_value.count].value = value;
        obj->object_value.count++;

        json_skip_spaces(text);
        if (**text == ',') {
            (*text)++;
            json_skip_spaces(text);
        }
    }

    if (**text == '}') (*text)++; // skip '}'
    return obj;

    error:
        json_free_value(obj);
        return NULL;
}

// returns NULL on error
json_value_t* json_read_value(const char** text) {
    json_value_t* val;
    val = malloc(sizeof(json_value_t));
    if (!val) return NULL;

    json_skip_spaces(text);

    if (**text == '"') {
        val->type = TYPE_STRING;
        val->string_value = json_read_string(text);
        if (!val->string_value) goto error_string;
        return val;
    } else if (isdigit(**text) || **text == '-') {
        val->type = TYPE_NUMBER;
        val->number_value = json_read_number(text);
        return val;
    } else if (strncmp(*text, "true", 4) == 0 || strncmp(*text, "false", 5) == 0) {
        val->type = TYPE_BOOL;
        val->bool_value = json_read_bool(text);
        return val;
    } else if (strncmp(*text, "null", 4) == 0) {
        val->type = TYPE_NULL;
        *text += 4;
        return val;
    } else if (**text == '{') {
        val = json_read_object(text);
        return val;
    }

    return NULL;

    error_string:
        json_free_value(val);
        return NULL;
}

// returns NULL on error
json_value_t* json_load_root(const char* filepath) {   
    FILE* f = fopen(filepath, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    off_t size = ftell(f);
    if (size < 0) goto error_file;
    rewind(f);

    char* data = malloc(size + 1);
    if (!data) goto error_data;

    size_t read = fread(data, 1, (size_t)size, f);
    if (read != (size_t)size || ferror(f)) goto error_data;
    data[size] = '\0';
    
    const char* p = data;
    json_value_t* result = json_read_value(&p);
    if (!result) goto error_data;

    free(data);
    fclose(f);
       
    return result;

    error_file:
        fclose(f);
        return NULL;
    
    error_data:
        free(data);
        fclose(f);
        return NULL;
}

void json_free_value(json_value_t* value) {
    if (!value) return;
        
    switch (value->type) {
        case TYPE_STRING:
            free(value->string_value);
            break;
        case TYPE_NUMBER:
            // pass
            break;
        case TYPE_BOOL:
            // pass
            break;
        case TYPE_NULL:
            break;
        case TYPE_OBJECT:
            for (size_t i = 0; i < value->object_value.count; ++i) {
                free(value->object_value.pairs[i].key);
                json_free_value(value->object_value.pairs[i].value);
            }
            free(value->object_value.pairs);
            break;
    }
    
    free(value);
}

// returns NULL on error
json_value_t* json_get_value(json_value_t* root, const char* path) {
    if (root == NULL || root->type != TYPE_OBJECT) return NULL;

    char* path_copy = strdup(path);
    char* token = strtok(path_copy, ".");
    json_value_t* current = root;

    while (token != NULL && current->type == TYPE_OBJECT) {
        int found = 0;
        for (size_t i = 0; i < current->object_value.count; i++) {
            if (strcmp(current->object_value.pairs[i].key, token) == 0) {
                current = current->object_value.pairs[i].value;
                found = 1;
                break;
            }
        }
        if (!found) {
            free(path_copy);
            return NULL;
        }
        token = strtok(NULL, ".");
    }

    free(path_copy);
    return current;
}

void json_print_value(json_value_t* value) {
    if (!value) {
        fprintf(stderr, "Json value not found.\n");
        return;
    }

    switch (value->type) {
        case TYPE_STRING:
            printf("String: %s\n", value->string_value);
            break;
        case TYPE_NUMBER:
            printf("Number: %.2f\n", value->number_value);
            break;
        case TYPE_BOOL:
            printf("Boolean: %s\n", value->bool_value ? "true" : "false");
            break;
        case TYPE_NULL:
            printf("Null\n");
            break;
        default:
            printf("Object {...}\n");
            break;
    }
}
