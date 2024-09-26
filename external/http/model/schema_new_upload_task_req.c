#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "schema_new_upload_task_req.h"



schema_new_upload_task_req_t *schema_new_upload_task_req_create(
    char *ref,
    char *repo_name
    ) {
    schema_new_upload_task_req_t *schema_new_upload_task_req_local_var = malloc(sizeof(schema_new_upload_task_req_t));
    if (!schema_new_upload_task_req_local_var) {
        return NULL;
    }
    schema_new_upload_task_req_local_var->ref = ref;
    schema_new_upload_task_req_local_var->repo_name = repo_name;

    return schema_new_upload_task_req_local_var;
}


void schema_new_upload_task_req_free(schema_new_upload_task_req_t *schema_new_upload_task_req) {
    if(NULL == schema_new_upload_task_req){
        return ;
    }
    listEntry_t *listEntry;
    if (schema_new_upload_task_req->ref) {
        free(schema_new_upload_task_req->ref);
        schema_new_upload_task_req->ref = NULL;
    }
    if (schema_new_upload_task_req->repo_name) {
        free(schema_new_upload_task_req->repo_name);
        schema_new_upload_task_req->repo_name = NULL;
    }
    free(schema_new_upload_task_req);
}

cJSON *schema_new_upload_task_req_convertToJSON(schema_new_upload_task_req_t *schema_new_upload_task_req) {
    cJSON *item = cJSON_CreateObject();

    // schema_new_upload_task_req->ref
    if(schema_new_upload_task_req->ref) {
    if(cJSON_AddStringToObject(item, "ref", schema_new_upload_task_req->ref) == NULL) {
    goto fail; //String
    }
    }


    // schema_new_upload_task_req->repo_name
    if(schema_new_upload_task_req->repo_name) {
    if(cJSON_AddStringToObject(item, "repoName", schema_new_upload_task_req->repo_name) == NULL) {
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

schema_new_upload_task_req_t *schema_new_upload_task_req_parseFromJSON(cJSON *schema_new_upload_task_reqJSON){

    schema_new_upload_task_req_t *schema_new_upload_task_req_local_var = NULL;

    // schema_new_upload_task_req->ref
    cJSON *ref = cJSON_GetObjectItemCaseSensitive(schema_new_upload_task_reqJSON, "ref");
    if (ref) { 
    if(!cJSON_IsString(ref) && !cJSON_IsNull(ref))
    {
    goto end; //String
    }
    }

    // schema_new_upload_task_req->repo_name
    cJSON *repo_name = cJSON_GetObjectItemCaseSensitive(schema_new_upload_task_reqJSON, "repoName");
    if (repo_name) { 
    if(!cJSON_IsString(repo_name) && !cJSON_IsNull(repo_name))
    {
    goto end; //String
    }
    }


    schema_new_upload_task_req_local_var = schema_new_upload_task_req_create (
        ref && !cJSON_IsNull(ref) ? strdup(ref->valuestring) : NULL,
        repo_name && !cJSON_IsNull(repo_name) ? strdup(repo_name->valuestring) : NULL
        );

    return schema_new_upload_task_req_local_var;
end:
    return NULL;

}
