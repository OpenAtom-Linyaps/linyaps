#ifndef request_auth_TEST
#define request_auth_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define request_auth_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/request_auth.h"
request_auth_t* instantiate_request_auth(int include_optional);



request_auth_t* instantiate_request_auth(int include_optional) {
  request_auth_t* request_auth = NULL;
  if (include_optional) {
    request_auth = request_auth_create(
      "ut12345678",
      "ut12345678"
    );
  } else {
    request_auth = request_auth_create(
      "ut12345678",
      "ut12345678"
    );
  }

  return request_auth;
}


#ifdef request_auth_MAIN

void test_request_auth(int include_optional) {
    request_auth_t* request_auth_1 = instantiate_request_auth(include_optional);

	cJSON* jsonrequest_auth_1 = request_auth_convertToJSON(request_auth_1);
	printf("request_auth :\n%s\n", cJSON_Print(jsonrequest_auth_1));
	request_auth_t* request_auth_2 = request_auth_parseFromJSON(jsonrequest_auth_1);
	cJSON* jsonrequest_auth_2 = request_auth_convertToJSON(request_auth_2);
	printf("repeating request_auth:\n%s\n", cJSON_Print(jsonrequest_auth_2));
}

int main() {
  test_request_auth(1);
  test_request_auth(0);

  printf("Hello world \n");
  return 0;
}

#endif // request_auth_MAIN
#endif // request_auth_TEST
