#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "response_upload_task_status_info.h"



response_upload_task_status_info_t *response_upload_task_status_info_create(
    char *status
    ) {
    response_upload_task_status_info_t *response_upload_task_status_info_local_var = malloc(sizeof(response_upload_task_status_info_t));
    if (!response_upload_task_status_info_local_var) {
        return NULL;
    }
    response_upload_task_status_info_local_var->status = status;

    return response_upload_task_status_info_local_var;
}


void response_upload_task_status_info_free(response_upload_task_status_info_t *response_upload_task_status_info) {
    if(NULL == response_upload_task_status_info){
        return ;
    }
    listEntry_t *listEntry;
    if (response_upload_task_status_info->status) {
        free(response_upload_task_status_info->status);
        response_upload_task_status_info->status = NULL;
    }
    free(response_upload_task_status_info);
}

cJSON *response_upload_task_status_info_convertToJSON(response_upload_task_status_info_t *response_upload_task_status_info) {
    cJSON *item = cJSON_CreateObject();

    // response_upload_task_status_info->status
    if(response_upload_task_status_info->status) {
    if(cJSON_AddStringToObject(item, "status", response_upload_task_status_info->status) == NULL) {
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

response_upload_task_status_info_t *response_upload_task_status_info_parseFromJSON(cJSON *response_upload_task_status_infoJSON){

    response_upload_task_status_info_t *response_upload_task_status_info_local_var = NULL;

    // response_upload_task_status_info->status
    cJSON *status = cJSON_GetObjectItemCaseSensitive(response_upload_task_status_infoJSON, "status");
    if (status) { 
    if(!cJSON_IsString(status) && !cJSON_IsNull(status))
    {
    goto end; //String
    }
    }


    response_upload_task_status_info_local_var = response_upload_task_status_info_create (
        status && !cJSON_IsNull(status) ? strdup(status->valuestring) : NULL
        );

    return response_upload_task_status_info_local_var;
end:
    return NULL;

}
