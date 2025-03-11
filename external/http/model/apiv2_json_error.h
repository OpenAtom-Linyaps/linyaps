/*
 * apiv2_json_error.h
 *
 * 
 */

#ifndef _apiv2_json_error_H_
#define _apiv2_json_error_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct apiv2_json_error_t apiv2_json_error_t;




typedef struct apiv2_json_error_t {
    int code; //numeric
    list_t* fields; //map
    char *msg; // string
    char *trace_id; // string

} apiv2_json_error_t;

apiv2_json_error_t *apiv2_json_error_create(
    int code,
    list_t* fields,
    char *msg,
    char *trace_id
);

void apiv2_json_error_free(apiv2_json_error_t *apiv2_json_error);

apiv2_json_error_t *apiv2_json_error_parseFromJSON(cJSON *apiv2_json_errorJSON);

cJSON *apiv2_json_error_convertToJSON(apiv2_json_error_t *apiv2_json_error);

#endif /* _apiv2_json_error_H_ */

