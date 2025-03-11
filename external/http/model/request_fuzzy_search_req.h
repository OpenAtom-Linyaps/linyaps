/*
 * request_fuzzy_search_req.h
 *
 * 
 */

#ifndef _request_fuzzy_search_req_H_
#define _request_fuzzy_search_req_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct request_fuzzy_search_req_t request_fuzzy_search_req_t;




typedef struct request_fuzzy_search_req_t {
    char *app_id; // string
    char *arch; // string
    char *channel; // string
    char *repo_name; // string
    char *version; // string

} request_fuzzy_search_req_t;

request_fuzzy_search_req_t *request_fuzzy_search_req_create(
    char *app_id,
    char *arch,
    char *channel,
    char *repo_name,
    char *version
);

void request_fuzzy_search_req_free(request_fuzzy_search_req_t *request_fuzzy_search_req);

request_fuzzy_search_req_t *request_fuzzy_search_req_parseFromJSON(cJSON *request_fuzzy_search_reqJSON);

cJSON *request_fuzzy_search_req_convertToJSON(request_fuzzy_search_req_t *request_fuzzy_search_req);

#endif /* _request_fuzzy_search_req_H_ */

