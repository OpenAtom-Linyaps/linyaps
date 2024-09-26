/*
 * request_register_struct.h
 *
 * 
 */

#ifndef _request_register_struct_H_
#define _request_register_struct_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct request_register_struct_t request_register_struct_t;




typedef struct request_register_struct_t {
    char *app_id; // string
    char *arch; // string
    char *channel; // string
    char *description; // string
    char *kind; // string
    char *module; // string
    char *name; // string
    char *repo_name; // string
    char *runtime; // string
    int size; //numeric
    char *uab_url; // string
    char *version; // string

} request_register_struct_t;

request_register_struct_t *request_register_struct_create(
    char *app_id,
    char *arch,
    char *channel,
    char *description,
    char *kind,
    char *module,
    char *name,
    char *repo_name,
    char *runtime,
    int size,
    char *uab_url,
    char *version
);

void request_register_struct_free(request_register_struct_t *request_register_struct);

request_register_struct_t *request_register_struct_parseFromJSON(cJSON *request_register_structJSON);

cJSON *request_register_struct_convertToJSON(request_register_struct_t *request_register_struct);

#endif /* _request_register_struct_H_ */

