
#include "CUnit/Basic.h"

#include "components/edge-color-processor.h"
#include "components/edge-color-processor.r"

#include "edge-color-processor-test.h"

void test_edge_color_processor_point_to_box_0_0() {
   int width = 100;
   int height = 100;
   int x = 0;
   int y = 0;

   point_to_box(&x, &y, 4, 4, width, height);
   CU_ASSERT_EQUAL(x, 0);
   CU_ASSERT_EQUAL(y, 0);
}

void test_edge_color_processor_point_to_box_100_0() {
   int width = 100;
   int height = 100;
   int x = 100;
   int y = 0;

   point_to_box(&x, &y, 4, 4, width, height);
   CU_ASSERT_EQUAL(x, 96);
   CU_ASSERT_EQUAL(y, 0);
}

void test_edge_color_processor_point_to_box_1_1() {
   int width = 100;
   int height = 100;
   int x = 1;
   int y = 1;

   point_to_box(&x, &y, 4, 4, width, height);
   CU_ASSERT_EQUAL(x, 0);
   CU_ASSERT_EQUAL(y, 0);
}

void test_edge_color_processor_point_to_box_10_0() {
   int width = 100;
   int height = 100;
   int x = 10;
   int y = 0;

   point_to_box(&x, &y, 4, 4, width, height);
   CU_ASSERT_EQUAL(x, 6);
   CU_ASSERT_EQUAL(y, 0);
}

void test_edge_color_processor_point_to_box_0_100() {
   int width = 100;
   int height = 100;
   int x = 0;
   int y = 100;

   point_to_box(&x, &y, 4, 4, width, height);
   CU_ASSERT_EQUAL(x, 0);
   CU_ASSERT_EQUAL(y, 96);
}

int edge_color_processor_test_add_suite() {
   CU_pSuite pSuite = NULL;

   /* add a suite to the registry */
   pSuite = CU_add_suite("edge_color_processor", NULL, NULL);
   if (NULL == pSuite) {
      CU_cleanup_registry();
      return CU_get_error();
   }

   /* add the tests to the suite */
   if ((NULL == CU_add_test(pSuite, "test_edge_color_processor_point_to_box_0_0", test_edge_color_processor_point_to_box_0_0)) ||
       (NULL == CU_add_test(pSuite, "test_edge_color_processor_point_to_box_100_0", test_edge_color_processor_point_to_box_100_0)) ||
       (NULL == CU_add_test(pSuite, "test_edge_color_processor_point_to_box_1_1", test_edge_color_processor_point_to_box_1_1)) ||
       (NULL == CU_add_test(pSuite, "test_edge_color_processor_point_to_box_10_0", test_edge_color_processor_point_to_box_10_0)) ||
       (NULL == CU_add_test(pSuite, "test_edge_color_processor_point_to_box_0_100", test_edge_color_processor_point_to_box_0_100)) ||
         0
      ) {
      CU_cleanup_registry();
      return CU_get_error();
   }
   return CU_get_error();
}
