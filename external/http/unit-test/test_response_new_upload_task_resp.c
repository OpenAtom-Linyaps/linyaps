#ifndef response_new_upload_task_resp_TEST
#define response_new_upload_task_resp_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define response_new_upload_task_resp_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/response_new_upload_task_resp.h"
response_new_upload_task_resp_t* instantiate_response_new_upload_task_resp(int include_optional);



response_new_upload_task_resp_t* instantiate_response_new_upload_task_resp(int include_optional) {
  response_new_upload_task_resp_t* response_new_upload_task_resp = NULL;
  if (include_optional) {
    response_new_upload_task_resp = response_new_upload_task_resp_create(
      "0"
    );
  } else {
    response_new_upload_task_resp = response_new_upload_task_resp_create(
      "0"
    );
  }

  return response_new_upload_task_resp;
}


#ifdef response_new_upload_task_resp_MAIN

void test_response_new_upload_task_resp(int include_optional) {
    response_new_upload_task_resp_t* response_new_upload_task_resp_1 = instantiate_response_new_upload_task_resp(include_optional);

	cJSON* jsonresponse_new_upload_task_resp_1 = response_new_upload_task_resp_convertToJSON(response_new_upload_task_resp_1);
	printf("response_new_upload_task_resp :\n%s\n", cJSON_Print(jsonresponse_new_upload_task_resp_1));
	response_new_upload_task_resp_t* response_new_upload_task_resp_2 = response_new_upload_task_resp_parseFromJSON(jsonresponse_new_upload_task_resp_1);
	cJSON* jsonresponse_new_upload_task_resp_2 = response_new_upload_task_resp_convertToJSON(response_new_upload_task_resp_2);
	printf("repeating response_new_upload_task_resp:\n%s\n", cJSON_Print(jsonresponse_new_upload_task_resp_2));
}

int main() {
  test_response_new_upload_task_resp(1);
  test_response_new_upload_task_resp(0);

  printf("Hello world \n");
  return 0;
}

#endif // response_new_upload_task_resp_MAIN
#endif // response_new_upload_task_resp_TEST
