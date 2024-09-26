#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "new_upload_task_id_200_response.h"



new_upload_task_id_200_response_t *new_upload_task_id_200_response_create(
    int code,
    response_new_upload_task_resp_t *data,
    char *msg,
    char *trace_id
    ) {
    new_upload_task_id_200_response_t *new_upload_task_id_200_response_local_var = malloc(sizeof(new_upload_task_id_200_response_t));
    if (!new_upload_task_id_200_response_local_var) {
        return NULL;
    }
    new_upload_task_id_200_response_local_var->code = code;
    new_upload_task_id_200_response_local_var->data = data;
    new_upload_task_id_200_response_local_var->msg = msg;
    new_upload_task_id_200_response_local_var->trace_id = trace_id;

    return new_upload_task_id_200_response_local_var;
}


void new_upload_task_id_200_response_free(new_upload_task_id_200_response_t *new_upload_task_id_200_response) {
    if(NULL == new_upload_task_id_200_response){
        return ;
    }
    listEntry_t *listEntry;
    if (new_upload_task_id_200_response->data) {
        response_new_upload_task_resp_free(new_upload_task_id_200_response->data);
        new_upload_task_id_200_response->data = NULL;
    }
    if (new_upload_task_id_200_response->msg) {
        free(new_upload_task_id_200_response->msg);
        new_upload_task_id_200_response->msg = NULL;
    }
    if (new_upload_task_id_200_response->trace_id) {
        free(new_upload_task_id_200_response->trace_id);
        new_upload_task_id_200_response->trace_id = NULL;
    }
    free(new_upload_task_id_200_response);
}

cJSON *new_upload_task_id_200_response_convertToJSON(new_upload_task_id_200_response_t *new_upload_task_id_200_response) {
    cJSON *item = cJSON_CreateObject();

    // new_upload_task_id_200_response->code
    if(new_upload_task_id_200_response->code) {
    if(cJSON_AddNumberToObject(item, "code", new_upload_task_id_200_response->code) == NULL) {
    goto fail; //Numeric
    }
    }


    // new_upload_task_id_200_response->data
    if(new_upload_task_id_200_response->data) {
    cJSON *data_local_JSON = response_new_upload_task_resp_convertToJSON(new_upload_task_id_200_response->data);
    if(data_local_JSON == NULL) {
    goto fail; //model
    }
    cJSON_AddItemToObject(item, "data", data_local_JSON);
    if(item->child == NULL) {
    goto fail;
    }
    }


    // new_upload_task_id_200_response->msg
    if(new_upload_task_id_200_response->msg) {
    if(cJSON_AddStringToObject(item, "msg", new_upload_task_id_200_response->msg) == NULL) {
    goto fail; //String
    }
    }


    // new_upload_task_id_200_response->trace_id
    if(new_upload_task_id_200_response->trace_id) {
    if(cJSON_AddStringToObject(item, "trace_id", new_upload_task_id_200_response->trace_id) == NULL) {
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

new_upload_task_id_200_response_t *new_upload_task_id_200_response_parseFromJSON(cJSON *new_upload_task_id_200_responseJSON){

    new_upload_task_id_200_response_t *new_upload_task_id_200_response_local_var = NULL;

    // define the local variable for new_upload_task_id_200_response->data
    response_new_upload_task_resp_t *data_local_nonprim = NULL;

    // new_upload_task_id_200_response->code
    cJSON *code = cJSON_GetObjectItemCaseSensitive(new_upload_task_id_200_responseJSON, "code");
    if (code) { 
    if(!cJSON_IsNumber(code))
    {
    goto end; //Numeric
    }
    }

    // new_upload_task_id_200_response->data
    cJSON *data = cJSON_GetObjectItemCaseSensitive(new_upload_task_id_200_responseJSON, "data");
    if (data) { 
    data_local_nonprim = response_new_upload_task_resp_parseFromJSON(data); //nonprimitive
    }

    // new_upload_task_id_200_response->msg
    cJSON *msg = cJSON_GetObjectItemCaseSensitive(new_upload_task_id_200_responseJSON, "msg");
    if (msg) { 
    if(!cJSON_IsString(msg) && !cJSON_IsNull(msg))
    {
    goto end; //String
    }
    }

    // new_upload_task_id_200_response->trace_id
    cJSON *trace_id = cJSON_GetObjectItemCaseSensitive(new_upload_task_id_200_responseJSON, "trace_id");
    if (trace_id) { 
    if(!cJSON_IsString(trace_id) && !cJSON_IsNull(trace_id))
    {
    goto end; //String
    }
    }


    new_upload_task_id_200_response_local_var = new_upload_task_id_200_response_create (
        code ? code->valuedouble : 0,
        data ? data_local_nonprim : NULL,
        msg && !cJSON_IsNull(msg) ? strdup(msg->valuestring) : NULL,
        trace_id && !cJSON_IsNull(trace_id) ? strdup(trace_id->valuestring) : NULL
        );

    return new_upload_task_id_200_response_local_var;
end:
    if (data_local_nonprim) {
        response_new_upload_task_resp_free(data_local_nonprim);
        data_local_nonprim = NULL;
    }
    return NULL;

}
