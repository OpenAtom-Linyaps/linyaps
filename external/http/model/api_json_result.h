/*
 * api_json_result.h
 *
 * 
 */

#ifndef _api_json_result_H_
#define _api_json_result_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct api_json_result_t api_json_result_t;

#include "object.h"



typedef struct api_json_result_t {
    int code; //numeric
    object_t *data; //object
    char *msg; // string
    char *trace_id; // string

} api_json_result_t;

api_json_result_t *api_json_result_create(
    int code,
    object_t *data,
    char *msg,
    char *trace_id
);

void api_json_result_free(api_json_result_t *api_json_result);

api_json_result_t *api_json_result_parseFromJSON(cJSON *api_json_resultJSON);

cJSON *api_json_result_convertToJSON(api_json_result_t *api_json_result);

#endif /* _api_json_result_H_ */

