#pragma once

#include <stdlib.h>

typedef enum json_type {
    TYPE_STRING,
    TYPE_NUMBER,
    TYPE_BOOL,
    TYPE_NULL,
    TYPE_ARRAY,
    TYPE_OBJECT
} json_type_t;

struct json_value; // forward declaration

typedef struct json_pair {
    char* key;
    struct json_value* value;
} json_pair_t;

typedef struct json_value {
    json_type_t type;
    union {
        char* string_value;
        double number_value;
        int bool_value;
        struct {
            struct json_value** items;
            size_t count;
            size_t capacity;
        } array_value;
        struct {
            json_pair_t* pairs;
            size_t count;
            size_t capacity;
        } object_value;
    };
} json_value_t;

// returns root object from filepath
// returns null-filled object on error opening filepath
json_value_t* json_load_root(const char* filepath);

// frees a json_value_t with nested objects recursively
void json_free_value(json_value_t* value);

// searches for a json value in the given root object
// returns the value on finding it
// returns NULL on not finding it
// use dot notation for searching for values in nested objects
// example of nested object path: user.name
json_value_t* json_get_value(json_value_t* root, const char* path);

// prints a json_value_t in a readable manner
void json_print_value(json_value_t* value);
