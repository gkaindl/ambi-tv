
#include "CUnit/Basic.h"
#include "util.h"

void test_append_ptr_to_list_adding_an_entry() {
   void** list_of_ints = NULL;
   int ilen = 0;
   int idx = 0;
   int entry = 42;

   int next_idx = ambitv_util_append_ptr_to_list(
         &list_of_ints,
         idx,
         &ilen,
         (void*)entry
    );

   CU_ASSERT_EQUAL(next_idx, 1);
   CU_ASSERT_EQUAL(ilen, 4);
   CU_ASSERT_EQUAL((int)list_of_ints[0], entry);
}

void test_append_ptr_to_list_adding_another_entry() {
   void** list_of_ints = NULL;

   int ilen = 0;
   int idx = 0;
   int entry1 = 201;
   int entry2 = 202;

   int next_idx1 = ambitv_util_append_ptr_to_list(
         &list_of_ints,
         idx,
         &ilen,
         (void*)entry1
    );

   int next_idx2 = ambitv_util_append_ptr_to_list(
         &list_of_ints,
         next_idx1,
         &ilen,
         (void*)entry2
    );

   CU_ASSERT_EQUAL(next_idx2, 2);
   CU_ASSERT_EQUAL(ilen, 4);
   CU_ASSERT_EQUAL((int)list_of_ints[0], entry1);
   CU_ASSERT_EQUAL((int)list_of_ints[1], entry2);
}

void test_parse_led_string_range_ascending() {
   char* config_value = "1-3";
   int *led_str;
   void*** led_strp = (void***)&led_str;
   int led_len;
   int ret = ambitv_parse_led_string(config_value, &led_str, &led_len);

   CU_ASSERT_EQUAL(led_len, 3);
   CU_ASSERT_EQUAL((int)(*led_strp)[0], 1);
   CU_ASSERT_EQUAL((int)(*led_strp)[1], 2);
   CU_ASSERT_EQUAL((int)(*led_strp)[2], 3);
   CU_ASSERT_EQUAL(ret, 0);
}

void test_parse_led_string_range_descending() {
   char* config_value = "3-1";
   int *led_str;
   void*** led_strp = (void***)&led_str;
   int led_len;
   int ret = ambitv_parse_led_string(config_value, &led_str, &led_len);

   CU_ASSERT_EQUAL(led_len, 3);
   CU_ASSERT_EQUAL((int)(*led_strp)[0], 3);
   CU_ASSERT_EQUAL((int)(*led_strp)[1], 2);
   CU_ASSERT_EQUAL((int)(*led_strp)[2], 1);
   CU_ASSERT_EQUAL(ret, 0);
}

void test_parse_led_string_with_implicit_gaps() {
   char* config_value = "1,3";
   int *led_str;
   void*** led_strp = (void***)&led_str;
   int led_len;
   int ret = ambitv_parse_led_string(config_value, &led_str, &led_len);

   CU_ASSERT_EQUAL(led_len, 2);
   CU_ASSERT_EQUAL((int)(*led_strp)[0], 1);
   CU_ASSERT_EQUAL((int)(*led_strp)[1], 3);
   CU_ASSERT_EQUAL(ret, 0);
}

void test_parse_led_string_with_gaps() {
   char* config_value = "1,2X,4";
   int *led_str;
   void*** led_strp = (void***)&led_str;
   int led_len;
   int ret = ambitv_parse_led_string(config_value, &led_str, &led_len);

   CU_ASSERT_EQUAL(led_len, 4);
   CU_ASSERT_EQUAL((int)(*led_strp)[0], 1);
   CU_ASSERT_EQUAL((int)(*led_strp)[1], -1);
   CU_ASSERT_EQUAL((int)(*led_strp)[2], -1);
   CU_ASSERT_EQUAL((int)(*led_strp)[3], 4);
   CU_ASSERT_EQUAL(ret, 0);
}

int util_test_add_suite() {
   CU_pSuite pSuite = NULL;

   /* add a suite to the registry */
   pSuite = CU_add_suite("util", NULL, NULL);
   if (NULL == pSuite) {
      CU_cleanup_registry();
      return CU_get_error();
   }

   /* add the tests to the suite */
   if ((NULL == CU_add_test(pSuite, "test_append_ptr_to_list_adding_an_entry", test_append_ptr_to_list_adding_an_entry))  ||
       (NULL == CU_add_test(pSuite, "test_append_ptr_to_list_adding_another_entry", test_append_ptr_to_list_adding_another_entry)) ||
       (NULL == CU_add_test(pSuite, "test_parse_led_string_range_ascending", test_parse_led_string_range_ascending)) ||
       (NULL == CU_add_test(pSuite, "test_parse_led_string_range_descending", test_parse_led_string_range_descending)) ||
       (NULL == CU_add_test(pSuite, "test_parse_led_string_with_implicit_gaps", test_parse_led_string_with_implicit_gaps)) ||
       (NULL == CU_add_test(pSuite, "test_parse_led_string_with_gaps", test_parse_led_string_with_gaps))
      ) {
      CU_cleanup_registry();
      return CU_get_error();
   }
   return CU_get_error();
}

