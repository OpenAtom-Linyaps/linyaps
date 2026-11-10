/*
 * apiv2_search_app_response.h
 *
 *
 */

#ifndef _apiv2_search_app_response_H_
#define _apiv2_search_app_response_H_

#include "../external/cJSON.h"
#include "../include/binary.h"
#include "../include/keyValuePair.h"
#include "../include/list.h"

#include <string.h>

typedef struct apiv2_search_app_response_t apiv2_search_app_response_t;

#include "request_register_struct.h"

typedef struct apiv2_search_app_response_t
{
    int code;       // numeric
    list_t *data;   // nonprimitive container
    char *trace_id; // string

} apiv2_search_app_response_t;

apiv2_search_app_response_t *apiv2_search_app_response_create(int code,
                                                              list_t *data,
                                                              char *trace_id);

void apiv2_search_app_response_free(apiv2_search_app_response_t *apiv2_search_app_response);

apiv2_search_app_response_t *
apiv2_search_app_response_parseFromJSON(cJSON *apiv2_search_app_responseJSON);

cJSON *
apiv2_search_app_response_convertToJSON(apiv2_search_app_response_t *apiv2_search_app_response);

#endif /* _apiv2_search_app_response_H_ */
