/*
 * upload_task_info_200_response.h
 *
 * 
 */

#ifndef _upload_task_info_200_response_H_
#define _upload_task_info_200_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct upload_task_info_200_response_t upload_task_info_200_response_t;

#include "response_upload_task_status_info.h"



typedef struct upload_task_info_200_response_t {
    int code; //numeric
    struct response_upload_task_status_info_t *data; //model
    char *msg; // string
    char *trace_id; // string

} upload_task_info_200_response_t;

upload_task_info_200_response_t *upload_task_info_200_response_create(
    int code,
    response_upload_task_status_info_t *data,
    char *msg,
    char *trace_id
);

void upload_task_info_200_response_free(upload_task_info_200_response_t *upload_task_info_200_response);

upload_task_info_200_response_t *upload_task_info_200_response_parseFromJSON(cJSON *upload_task_info_200_responseJSON);

cJSON *upload_task_info_200_response_convertToJSON(upload_task_info_200_response_t *upload_task_info_200_response);

#endif /* _upload_task_info_200_response_H_ */

