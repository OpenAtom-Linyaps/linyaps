#ifndef new_upload_task_id_200_response_TEST
#define new_upload_task_id_200_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define new_upload_task_id_200_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/new_upload_task_id_200_response.h"
new_upload_task_id_200_response_t* instantiate_new_upload_task_id_200_response(int include_optional);

#include "test_response_new_upload_task_resp.c"


new_upload_task_id_200_response_t* instantiate_new_upload_task_id_200_response(int include_optional) {
  new_upload_task_id_200_response_t* new_upload_task_id_200_response = NULL;
  if (include_optional) {
    new_upload_task_id_200_response = new_upload_task_id_200_response_create(
      56,
       // false, not to have infinite recursion
      instantiate_response_new_upload_task_resp(0),
      "0",
      "0"
    );
  } else {
    new_upload_task_id_200_response = new_upload_task_id_200_response_create(
      56,
      NULL,
      "0",
      "0"
    );
  }

  return new_upload_task_id_200_response;
}


#ifdef new_upload_task_id_200_response_MAIN

void test_new_upload_task_id_200_response(int include_optional) {
    new_upload_task_id_200_response_t* new_upload_task_id_200_response_1 = instantiate_new_upload_task_id_200_response(include_optional);

	cJSON* jsonnew_upload_task_id_200_response_1 = new_upload_task_id_200_response_convertToJSON(new_upload_task_id_200_response_1);
	printf("new_upload_task_id_200_response :\n%s\n", cJSON_Print(jsonnew_upload_task_id_200_response_1));
	new_upload_task_id_200_response_t* new_upload_task_id_200_response_2 = new_upload_task_id_200_response_parseFromJSON(jsonnew_upload_task_id_200_response_1);
	cJSON* jsonnew_upload_task_id_200_response_2 = new_upload_task_id_200_response_convertToJSON(new_upload_task_id_200_response_2);
	printf("repeating new_upload_task_id_200_response:\n%s\n", cJSON_Print(jsonnew_upload_task_id_200_response_2));
}

int main() {
  test_new_upload_task_id_200_response(1);
  test_new_upload_task_id_200_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // new_upload_task_id_200_response_MAIN
#endif // new_upload_task_id_200_response_TEST
