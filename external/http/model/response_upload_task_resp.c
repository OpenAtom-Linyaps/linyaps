#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "response_upload_task_resp.h"



response_upload_task_resp_t *response_upload_task_resp_create(
    char *watch_id
    ) {
    response_upload_task_resp_t *response_upload_task_resp_local_var = malloc(sizeof(response_upload_task_resp_t));
    if (!response_upload_task_resp_local_var) {
        return NULL;
    }
    response_upload_task_resp_local_var->watch_id = watch_id;

    return response_upload_task_resp_local_var;
}


void response_upload_task_resp_free(response_upload_task_resp_t *response_upload_task_resp) {
    if(NULL == response_upload_task_resp){
        return ;
    }
    listEntry_t *listEntry;
    if (response_upload_task_resp->watch_id) {
        free(response_upload_task_resp->watch_id);
        response_upload_task_resp->watch_id = NULL;
    }
    free(response_upload_task_resp);
}

cJSON *response_upload_task_resp_convertToJSON(response_upload_task_resp_t *response_upload_task_resp) {
    cJSON *item = cJSON_CreateObject();

    // response_upload_task_resp->watch_id
    if(response_upload_task_resp->watch_id) {
    if(cJSON_AddStringToObject(item, "watchId", response_upload_task_resp->watch_id) == NULL) {
    goto fail; //String
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

response_upload_task_resp_t *response_upload_task_resp_parseFromJSON(cJSON *response_upload_task_respJSON){

    response_upload_task_resp_t *response_upload_task_resp_local_var = NULL;

    // response_upload_task_resp->watch_id
    cJSON *watch_id = cJSON_GetObjectItemCaseSensitive(response_upload_task_respJSON, "watchId");
    if (watch_id) { 
    if(!cJSON_IsString(watch_id) && !cJSON_IsNull(watch_id))
    {
    goto end; //String
    }
    }


    response_upload_task_resp_local_var = response_upload_task_resp_create (
        watch_id && !cJSON_IsNull(watch_id) ? strdup(watch_id->valuestring) : NULL
        );

    return response_upload_task_resp_local_var;
end:
    return NULL;

}
