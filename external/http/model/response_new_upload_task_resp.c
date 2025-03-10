#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "response_new_upload_task_resp.h"



response_new_upload_task_resp_t *response_new_upload_task_resp_create(
    char *id
    ) {
    response_new_upload_task_resp_t *response_new_upload_task_resp_local_var = malloc(sizeof(response_new_upload_task_resp_t));
    if (!response_new_upload_task_resp_local_var) {
        return NULL;
    }
    response_new_upload_task_resp_local_var->id = id;

    return response_new_upload_task_resp_local_var;
}


void response_new_upload_task_resp_free(response_new_upload_task_resp_t *response_new_upload_task_resp) {
    if(NULL == response_new_upload_task_resp){
        return ;
    }
    listEntry_t *listEntry;
    if (response_new_upload_task_resp->id) {
        free(response_new_upload_task_resp->id);
        response_new_upload_task_resp->id = NULL;
    }
    free(response_new_upload_task_resp);
}

cJSON *response_new_upload_task_resp_convertToJSON(response_new_upload_task_resp_t *response_new_upload_task_resp) {
    cJSON *item = cJSON_CreateObject();

    // response_new_upload_task_resp->id
    if(response_new_upload_task_resp->id) {
    if(cJSON_AddStringToObject(item, "id", response_new_upload_task_resp->id) == NULL) {
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

response_new_upload_task_resp_t *response_new_upload_task_resp_parseFromJSON(cJSON *response_new_upload_task_respJSON){

    response_new_upload_task_resp_t *response_new_upload_task_resp_local_var = NULL;

    // response_new_upload_task_resp->id
    cJSON *id = cJSON_GetObjectItemCaseSensitive(response_new_upload_task_respJSON, "id");
    if (id) { 
    if(!cJSON_IsString(id) && !cJSON_IsNull(id))
    {
    goto end; //String
    }
    }


    response_new_upload_task_resp_local_var = response_new_upload_task_resp_create (
        id && !cJSON_IsNull(id) ? strdup(id->valuestring) : NULL
        );

    return response_new_upload_task_resp_local_var;
end:
    return NULL;

}
