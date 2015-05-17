
#include <stdio.h>
#include "CUnit/Basic.h"
#include "CUnit/Console.h"
#include "CUnit/Automated.h"

#include "util-test.h"
#include "lpd8806-spidev-sink-test.h"
#include "edge-color-processor-test.h"
#include "video-fmt-test.h"

int main()
{
   /* initialize the CUnit test registry */
   if (CUE_SUCCESS != CU_initialize_registry())
      return CU_get_error();

   int retval;

   retval = lpd8806_spidev_sink_test_add_suite();
   if (retval != CUE_SUCCESS) {
    return retval;
   }

   retval = util_test_add_suite();
   if (retval != CUE_SUCCESS) {
    return retval;
   }
 
   retval = edge_color_processor_test_add_suite();
   if (retval != CUE_SUCCESS) {
    return retval;
   }

   retval = video_fmt_test_add_suite();
   if (retval != CUE_SUCCESS) {
    return retval;
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
