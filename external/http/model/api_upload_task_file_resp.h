/*
 * api_upload_task_file_resp.h
 *
 * 
 */

#ifndef _api_upload_task_file_resp_H_
#define _api_upload_task_file_resp_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct api_upload_task_file_resp_t api_upload_task_file_resp_t;

#include "response_upload_task_resp.h"



typedef struct api_upload_task_file_resp_t {
    int code; //numeric
    struct response_upload_task_resp_t *data; //model
    char *msg; // string
    char *trace_id; // string

} api_upload_task_file_resp_t;

api_upload_task_file_resp_t *api_upload_task_file_resp_create(
    int code,
    response_upload_task_resp_t *data,
    char *msg,
    char *trace_id
);

void api_upload_task_file_resp_free(api_upload_task_file_resp_t *api_upload_task_file_resp);

api_upload_task_file_resp_t *api_upload_task_file_resp_parseFromJSON(cJSON *api_upload_task_file_respJSON);

cJSON *api_upload_task_file_resp_convertToJSON(api_upload_task_file_resp_t *api_upload_task_file_resp);

#endif /* _api_upload_task_file_resp_H_ */

