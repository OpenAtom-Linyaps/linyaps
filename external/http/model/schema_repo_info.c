#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "schema_repo_info.h"



schema_repo_info_t *schema_repo_info_create(
    char *mode,
    char *name,
    list_t *refs
    ) {
    schema_repo_info_t *schema_repo_info_local_var = malloc(sizeof(schema_repo_info_t));
    if (!schema_repo_info_local_var) {
        return NULL;
    }
    schema_repo_info_local_var->mode = mode;
    schema_repo_info_local_var->name = name;
    schema_repo_info_local_var->refs = refs;

    return schema_repo_info_local_var;
}


void schema_repo_info_free(schema_repo_info_t *schema_repo_info) {
    if(NULL == schema_repo_info){
        return ;
    }
    listEntry_t *listEntry;
    if (schema_repo_info->mode) {
        free(schema_repo_info->mode);
        schema_repo_info->mode = NULL;
    }
    if (schema_repo_info->name) {
        free(schema_repo_info->name);
        schema_repo_info->name = NULL;
    }
    if (schema_repo_info->refs) {
        list_ForEach(listEntry, schema_repo_info->refs) {
            free(listEntry->data);
        }
        list_freeList(schema_repo_info->refs);
        schema_repo_info->refs = NULL;
    }
    free(schema_repo_info);
}

cJSON *schema_repo_info_convertToJSON(schema_repo_info_t *schema_repo_info) {
    cJSON *item = cJSON_CreateObject();

    // schema_repo_info->mode
    if(schema_repo_info->mode) {
    if(cJSON_AddStringToObject(item, "mode", schema_repo_info->mode) == NULL) {
    goto fail; //String
    }
    }


    // schema_repo_info->name
    if(schema_repo_info->name) {
    if(cJSON_AddStringToObject(item, "name", schema_repo_info->name) == NULL) {
    goto fail; //String
    }
    }


    // schema_repo_info->refs
    if(schema_repo_info->refs) {
    cJSON *refs = cJSON_AddArrayToObject(item, "refs");
    if(refs == NULL) {
        goto fail; //primitive container
    }

    listEntry_t *refsListEntry;
    list_ForEach(refsListEntry, schema_repo_info->refs) {
    if(cJSON_AddStringToObject(refs, "", (char*)refsListEntry->data) == NULL)
    {
        goto fail;
    }
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

schema_repo_info_t *schema_repo_info_parseFromJSON(cJSON *schema_repo_infoJSON){

    schema_repo_info_t *schema_repo_info_local_var = NULL;

    // define the local list for schema_repo_info->refs
    list_t *refsList = NULL;

    // schema_repo_info->mode
    cJSON *mode = cJSON_GetObjectItemCaseSensitive(schema_repo_infoJSON, "mode");
    if (mode) { 
    if(!cJSON_IsString(mode) && !cJSON_IsNull(mode))
    {
    goto end; //String
    }
    }

    // schema_repo_info->name
    cJSON *name = cJSON_GetObjectItemCaseSensitive(schema_repo_infoJSON, "name");
    if (name) { 
    if(!cJSON_IsString(name) && !cJSON_IsNull(name))
    {
    goto end; //String
    }
    }

    // schema_repo_info->refs
    cJSON *refs = cJSON_GetObjectItemCaseSensitive(schema_repo_infoJSON, "refs");
    if (refs) { 
    cJSON *refs_local = NULL;
    if(!cJSON_IsArray(refs)) {
        goto end;//primitive container
    }
    refsList = list_createList();

    cJSON_ArrayForEach(refs_local, refs)
    {
        if(!cJSON_IsString(refs_local))
        {
            goto end;
        }
        list_addElement(refsList , strdup(refs_local->valuestring));
    }
    }


    schema_repo_info_local_var = schema_repo_info_create (
        mode && !cJSON_IsNull(mode) ? strdup(mode->valuestring) : NULL,
        name && !cJSON_IsNull(name) ? strdup(name->valuestring) : NULL,
        refs ? refsList : NULL
        );

    return schema_repo_info_local_var;
end:
    if (refsList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, refsList) {
            free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(refsList);
        refsList = NULL;
    }
    return NULL;

}
