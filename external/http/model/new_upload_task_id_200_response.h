/*
 * new_upload_task_id_200_response.h
 *
 * 
 */

#ifndef _new_upload_task_id_200_response_H_
#define _new_upload_task_id_200_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct new_upload_task_id_200_response_t new_upload_task_id_200_response_t;

#include "response_new_upload_task_resp.h"



typedef struct new_upload_task_id_200_response_t {
    int code; //numeric
    struct response_new_upload_task_resp_t *data; //model
    char *msg; // string
    char *trace_id; // string

} new_upload_task_id_200_response_t;

new_upload_task_id_200_response_t *new_upload_task_id_200_response_create(
    int code,
    response_new_upload_task_resp_t *data,
    char *msg,
    char *trace_id
);

void new_upload_task_id_200_response_free(new_upload_task_id_200_response_t *new_upload_task_id_200_response);

new_upload_task_id_200_response_t *new_upload_task_id_200_response_parseFromJSON(cJSON *new_upload_task_id_200_responseJSON);

cJSON *new_upload_task_id_200_response_convertToJSON(new_upload_task_id_200_response_t *new_upload_task_id_200_response);

#endif /* _new_upload_task_id_200_response_H_ */

