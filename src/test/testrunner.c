#include "CUnit/Basic.h"
#include "CUnit/Console.h"
#include "CUnit/Automated.h"

#define TRUE 1
#define FALSE 0

int init_suite_success(void) { return 0; }
int clean_suite_success(void) { return 0; }

void test_success1(void)
{
   CU_ASSERT(TRUE);
}

void test_success2(void)
{
   CU_ASSERT_NOT_EQUAL(2, -1);
}

void test_success3(void)
{
   CU_ASSERT_STRING_EQUAL("string #1", "string #1");
}

void test_success4(void)
{
   CU_ASSERT_STRING_NOT_EQUAL("string #1", "string #2");
}

void test_failure1(void)
{
   CU_ASSERT(FALSE);
}

void test_failure2(void)
{
   CU_ASSERT_EQUAL(2, 3);
}

void test_failure3(void)
{
   CU_ASSERT_STRING_NOT_EQUAL("string #1", "string #1");
}

void test_failure4(void)
{
   CU_ASSERT_STRING_EQUAL("string #1", "string #2");
}


int main()
{
   CU_pSuite pSuite = NULL;

   /* initialize the CUnit test registry */
   if (CUE_SUCCESS != CU_initialize_registry())
      return CU_get_error();

   /* add a suite to the registry */
   pSuite = CU_add_suite("Suite_mixed", NULL, NULL);
   if (NULL == pSuite) {
      CU_cleanup_registry();
      return CU_get_error();
   }

   /* add the tests to the suite */
   if ((NULL == CU_add_test(pSuite, "successful_test_2", test_success2)) ||
       (NULL == CU_add_test(pSuite, "failed_test_4",     test_failure4)) ||
       (NULL == CU_add_test(pSuite, "failed_test_2",     test_failure2)) ||
       (NULL == CU_add_test(pSuite, "successful_test_4", test_success4)))
   {
      CU_cleanup_registry();
      return CU_get_error();
   }

   /* Run all tests using the basic interface */
   CU_basic_set_mode(CU_BRM_VERBOSE);
   CU_basic_run_tests();
   printf("\n");
   CU_basic_show_failures(CU_get_failure_list());
   printf("\n\n");

   /* Clean up registry and return */
   CU_cleanup_registry();
   return CU_get_error();
}
