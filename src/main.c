#include "json_parser.h"
#include <stdio.h>


int json_example() {
    printf("Json Parser Example:\n\n");
    
    // load the root object from a file
    json_value_t* root = json_load_root("data/data1.json");
    if (!root) {
        fprintf(stderr, "File not found.\n");
        return 1;
    }

    // search for json values inside the root and output results
    printf("Looking for details.name:\n");
    json_value_t* name = json_get_value(root, "details.name");
    json_print_value(name);

    printf("Looking for details.age:\n");
    json_value_t* age = json_get_value(root, "details.age");
    json_print_value(age);

    printf("Looking for active:\n");
    json_value_t* active = json_get_value(root, "active");
    json_print_value(active);
    
    printf("Looking for numbers[3]:\n");
    json_value_t* numbers = json_get_value(root, "numbers[3]");
    json_print_value(numbers);

    // free root object including nested objects
    json_free_value(root);
    
    return 0;
}

int xml_example() {
    return 0;
}

int csv_example() {
    return 0;
}

int toml_example() {
    return 0;
}

int main() {
    if (json_example() != 0) return 1;
    if (xml_example() != 0) return 2;
    if (csv_example() != 0) return 3;
    if (toml_example() != 0) return 4;
    return 0;
}
