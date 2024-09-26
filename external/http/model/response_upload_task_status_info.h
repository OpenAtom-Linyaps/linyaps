/*
 * response_upload_task_status_info.h
 *
 * 
 */

#ifndef _response_upload_task_status_info_H_
#define _response_upload_task_status_info_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct response_upload_task_status_info_t response_upload_task_status_info_t;




typedef struct response_upload_task_status_info_t {
    char *status; // string

} response_upload_task_status_info_t;

response_upload_task_status_info_t *response_upload_task_status_info_create(
    char *status
);

void response_upload_task_status_info_free(response_upload_task_status_info_t *response_upload_task_status_info);

response_upload_task_status_info_t *response_upload_task_status_info_parseFromJSON(cJSON *response_upload_task_status_infoJSON);

cJSON *response_upload_task_status_info_convertToJSON(response_upload_task_status_info_t *response_upload_task_status_info);

#endif /* _response_upload_task_status_info_H_ */

