/*
 * get_repo_200_response.h
 *
 * 
 */

#ifndef _get_repo_200_response_H_
#define _get_repo_200_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct get_repo_200_response_t get_repo_200_response_t;

#include "schema_repo_info.h"



typedef struct get_repo_200_response_t {
    int code; //numeric
    struct schema_repo_info_t *data; //model
    char *msg; // string
    char *trace_id; // string

} get_repo_200_response_t;

get_repo_200_response_t *get_repo_200_response_create(
    int code,
    schema_repo_info_t *data,
    char *msg,
    char *trace_id
);

void get_repo_200_response_free(get_repo_200_response_t *get_repo_200_response);

get_repo_200_response_t *get_repo_200_response_parseFromJSON(cJSON *get_repo_200_responseJSON);

cJSON *get_repo_200_response_convertToJSON(get_repo_200_response_t *get_repo_200_response);

#endif /* _get_repo_200_response_H_ */

