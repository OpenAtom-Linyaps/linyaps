#ifndef apiv2_search_app_response_TEST
#define apiv2_search_app_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define apiv2_search_app_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/apiv2_search_app_response.h"
apiv2_search_app_response_t* instantiate_apiv2_search_app_response(int include_optional);



apiv2_search_app_response_t* instantiate_apiv2_search_app_response(int include_optional) {
  apiv2_search_app_response_t* apiv2_search_app_response = NULL;
  if (include_optional) {
    apiv2_search_app_response = apiv2_search_app_response_create(
      56,
      list_createList(),
      "0"
    );
  } else {
    apiv2_search_app_response = apiv2_search_app_response_create(
      56,
      list_createList(),
      "0"
    );
  }

  return apiv2_search_app_response;
}


#ifdef apiv2_search_app_response_MAIN

void test_apiv2_search_app_response(int include_optional) {
    apiv2_search_app_response_t* apiv2_search_app_response_1 = instantiate_apiv2_search_app_response(include_optional);

	cJSON* jsonapiv2_search_app_response_1 = apiv2_search_app_response_convertToJSON(apiv2_search_app_response_1);
	printf("apiv2_search_app_response :\n%s\n", cJSON_Print(jsonapiv2_search_app_response_1));
	apiv2_search_app_response_t* apiv2_search_app_response_2 = apiv2_search_app_response_parseFromJSON(jsonapiv2_search_app_response_1);
	cJSON* jsonapiv2_search_app_response_2 = apiv2_search_app_response_convertToJSON(apiv2_search_app_response_2);
	printf("repeating apiv2_search_app_response:\n%s\n", cJSON_Print(jsonapiv2_search_app_response_2));
}

int main() {
  test_apiv2_search_app_response(1);
  test_apiv2_search_app_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // apiv2_search_app_response_MAIN
#endif // apiv2_search_app_response_TEST
