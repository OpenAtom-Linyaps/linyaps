/*
 * _api_v1_repos_get_200_response.h
 *
 * 
 */

#ifndef __api_v1_repos_get_200_response_H_
#define __api_v1_repos_get_200_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct _api_v1_repos_get_200_response_t _api_v1_repos_get_200_response_t;

#include "schema_repo_info.h"



typedef struct _api_v1_repos_get_200_response_t {
    int code; //numeric
    list_t *data; //nonprimitive container
    char *msg; // string
    char *trace_id; // string

} _api_v1_repos_get_200_response_t;

_api_v1_repos_get_200_response_t *_api_v1_repos_get_200_response_create(
    int code,
    list_t *data,
    char *msg,
    char *trace_id
);

void _api_v1_repos_get_200_response_free(_api_v1_repos_get_200_response_t *_api_v1_repos_get_200_response);

_api_v1_repos_get_200_response_t *_api_v1_repos_get_200_response_parseFromJSON(cJSON *_api_v1_repos_get_200_responseJSON);

cJSON *_api_v1_repos_get_200_response_convertToJSON(_api_v1_repos_get_200_response_t *_api_v1_repos_get_200_response);

#endif /* __api_v1_repos_get_200_response_H_ */

