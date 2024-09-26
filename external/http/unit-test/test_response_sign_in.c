#ifndef response_sign_in_TEST
#define response_sign_in_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define response_sign_in_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/response_sign_in.h"
response_sign_in_t* instantiate_response_sign_in(int include_optional);



response_sign_in_t* instantiate_response_sign_in(int include_optional) {
  response_sign_in_t* response_sign_in = NULL;
  if (include_optional) {
    response_sign_in = response_sign_in_create(
      "0"
    );
  } else {
    response_sign_in = response_sign_in_create(
      "0"
    );
  }

  return response_sign_in;
}


#ifdef response_sign_in_MAIN

void test_response_sign_in(int include_optional) {
    response_sign_in_t* response_sign_in_1 = instantiate_response_sign_in(include_optional);

	cJSON* jsonresponse_sign_in_1 = response_sign_in_convertToJSON(response_sign_in_1);
	printf("response_sign_in :\n%s\n", cJSON_Print(jsonresponse_sign_in_1));
	response_sign_in_t* response_sign_in_2 = response_sign_in_parseFromJSON(jsonresponse_sign_in_1);
	cJSON* jsonresponse_sign_in_2 = response_sign_in_convertToJSON(response_sign_in_2);
	printf("repeating response_sign_in:\n%s\n", cJSON_Print(jsonresponse_sign_in_2));
}

int main() {
  test_response_sign_in(1);
  test_response_sign_in(0);

  printf("Hello world \n");
  return 0;
}

#endif // response_sign_in_MAIN
#endif // response_sign_in_TEST
