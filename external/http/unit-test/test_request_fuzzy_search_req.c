#ifndef request_fuzzy_search_req_TEST
#define request_fuzzy_search_req_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define request_fuzzy_search_req_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/request_fuzzy_search_req.h"
request_fuzzy_search_req_t* instantiate_request_fuzzy_search_req(int include_optional);



request_fuzzy_search_req_t* instantiate_request_fuzzy_search_req(int include_optional) {
  request_fuzzy_search_req_t* request_fuzzy_search_req = NULL;
  if (include_optional) {
    request_fuzzy_search_req = request_fuzzy_search_req_create(
      "0",
      "0",
      "0",
      "0",
      "0"
    );
  } else {
    request_fuzzy_search_req = request_fuzzy_search_req_create(
      "0",
      "0",
      "0",
      "0",
      "0"
    );
  }

  return request_fuzzy_search_req;
}


#ifdef request_fuzzy_search_req_MAIN

void test_request_fuzzy_search_req(int include_optional) {
    request_fuzzy_search_req_t* request_fuzzy_search_req_1 = instantiate_request_fuzzy_search_req(include_optional);

	cJSON* jsonrequest_fuzzy_search_req_1 = request_fuzzy_search_req_convertToJSON(request_fuzzy_search_req_1);
	printf("request_fuzzy_search_req :\n%s\n", cJSON_Print(jsonrequest_fuzzy_search_req_1));
	request_fuzzy_search_req_t* request_fuzzy_search_req_2 = request_fuzzy_search_req_parseFromJSON(jsonrequest_fuzzy_search_req_1);
	cJSON* jsonrequest_fuzzy_search_req_2 = request_fuzzy_search_req_convertToJSON(request_fuzzy_search_req_2);
	printf("repeating request_fuzzy_search_req:\n%s\n", cJSON_Print(jsonrequest_fuzzy_search_req_2));
}

int main() {
  test_request_fuzzy_search_req(1);
  test_request_fuzzy_search_req(0);

  printf("Hello world \n");
  return 0;
}

#endif // request_fuzzy_search_req_MAIN
#endif // request_fuzzy_search_req_TEST
