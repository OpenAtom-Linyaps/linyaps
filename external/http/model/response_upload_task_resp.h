/*
 * response_upload_task_resp.h
 *
 * 
 */

#ifndef _response_upload_task_resp_H_
#define _response_upload_task_resp_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct response_upload_task_resp_t response_upload_task_resp_t;




typedef struct response_upload_task_resp_t {
    char *watch_id; // string

} response_upload_task_resp_t;

response_upload_task_resp_t *response_upload_task_resp_create(
    char *watch_id
);

void response_upload_task_resp_free(response_upload_task_resp_t *response_upload_task_resp);

response_upload_task_resp_t *response_upload_task_resp_parseFromJSON(cJSON *response_upload_task_respJSON);

cJSON *response_upload_task_resp_convertToJSON(response_upload_task_resp_t *response_upload_task_resp);

#endif /* _response_upload_task_resp_H_ */

