/*
 * schema_new_upload_task_req.h
 *
 * 
 */

#ifndef _schema_new_upload_task_req_H_
#define _schema_new_upload_task_req_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct schema_new_upload_task_req_t schema_new_upload_task_req_t;




typedef struct schema_new_upload_task_req_t {
    char *ref; // string
    char *repo_name; // string

} schema_new_upload_task_req_t;

schema_new_upload_task_req_t *schema_new_upload_task_req_create(
    char *ref,
    char *repo_name
);

void schema_new_upload_task_req_free(schema_new_upload_task_req_t *schema_new_upload_task_req);

schema_new_upload_task_req_t *schema_new_upload_task_req_parseFromJSON(cJSON *schema_new_upload_task_reqJSON);

cJSON *schema_new_upload_task_req_convertToJSON(schema_new_upload_task_req_t *schema_new_upload_task_req);

#endif /* _schema_new_upload_task_req_H_ */

