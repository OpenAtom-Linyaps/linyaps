/*
 * response_new_upload_task_resp.h
 *
 * 
 */

#ifndef _response_new_upload_task_resp_H_
#define _response_new_upload_task_resp_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct response_new_upload_task_resp_t response_new_upload_task_resp_t;




typedef struct response_new_upload_task_resp_t {
    char *id; // string

} response_new_upload_task_resp_t;

response_new_upload_task_resp_t *response_new_upload_task_resp_create(
    char *id
);

void response_new_upload_task_resp_free(response_new_upload_task_resp_t *response_new_upload_task_resp);

response_new_upload_task_resp_t *response_new_upload_task_resp_parseFromJSON(cJSON *response_new_upload_task_respJSON);

cJSON *response_new_upload_task_resp_convertToJSON(response_new_upload_task_resp_t *response_new_upload_task_resp);

#endif /* _response_new_upload_task_resp_H_ */

