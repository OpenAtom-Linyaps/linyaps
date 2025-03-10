/*
 * fuzzy_search_app_200_response.h
 *
 * 
 */

#ifndef _fuzzy_search_app_200_response_H_
#define _fuzzy_search_app_200_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct fuzzy_search_app_200_response_t fuzzy_search_app_200_response_t;

#include "request_register_struct.h"



typedef struct fuzzy_search_app_200_response_t {
    int code; //numeric
    list_t *data; //nonprimitive container
    char *msg; // string
    char *trace_id; // string

} fuzzy_search_app_200_response_t;

fuzzy_search_app_200_response_t *fuzzy_search_app_200_response_create(
    int code,
    list_t *data,
    char *msg,
    char *trace_id
);

void fuzzy_search_app_200_response_free(fuzzy_search_app_200_response_t *fuzzy_search_app_200_response);

fuzzy_search_app_200_response_t *fuzzy_search_app_200_response_parseFromJSON(cJSON *fuzzy_search_app_200_responseJSON);

cJSON *fuzzy_search_app_200_response_convertToJSON(fuzzy_search_app_200_response_t *fuzzy_search_app_200_response);

#endif /* _fuzzy_search_app_200_response_H_ */

