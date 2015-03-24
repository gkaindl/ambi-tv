
#include <stdlib.h>

#include "CUnit/Basic.h"

#include "component.h"
#include "components/lpd8806-spidev-sink.h"
#include "components/lpd8806-spidev-sink.r"

/*
set tabstop=6 softtabstop=0 expandtab shiftwidth=3 smarttab
*/

int init_suite(void) {
   return 0;
}
int clean_suite(void) {
   return 0;
}

static struct ambitv_sink_component* SINK;
static int DISPLAY_WIDTH = 100;
static int DISPLAY_HEIGHT = 100;

static const int _NUM_LEDS = 40;
static const int _LED_LEN[] = { 10, 10, 10, 10 };
static const int _LED_STR_TOP[]    = { 1 };
static const int _LED_STR_BOTTOM[] = { 2 };
static const int _LED_STR_LEFT[]   = { 3 };
static const int _LED_STR_RIGHT[]  = { 4 };
static const int *_LED_STR[] = { _LED_STR_TOP, _LED_STR_BOTTOM, _LED_STR_LEFT, _LED_STR_RIGHT };
static const int _LED_INSET[] = { 0., 0., 0., 0.};

void set_up_sink() {
   SINK = (struct ambitv_sink_component*) malloc(sizeof(struct ambitv_sink_component));
   struct ambitv_lpd8806_priv* priv = (struct ambitv_lpd8806_priv*) malloc(sizeof(struct ambitv_lpd8806_priv));
   SINK->priv = priv;

   priv->num_leds  = _NUM_LEDS;
   memcpy(priv->led_len, _LED_LEN, sizeof(_LED_LEN));
   memcpy(priv->led_str, _LED_STR, sizeof(_LED_STR));
   memcpy(priv->led_inset, _LED_INSET, sizeof(_LED_INSET));
}

void tear_down_sink() {
   free(SINK->priv);
   free(SINK);
   SINK = NULL;
}

void set_up() {
   set_up_sink();
}

void tear_down() {
   tear_down_sink();
}

void test_lpd8806_map_output_to_point_zero(void)
{
   set_up();

   int output = 0; 
   int x = -1;
   int y = -1;
   int retval = ambitv_lpd8806_map_output_to_point(SINK, output, DISPLAY_WIDTH, DISPLAY_HEIGHT, &x, &y);
   CU_ASSERT_EQUAL(x, 0);
   CU_ASSERT_EQUAL(y, 0);
   CU_ASSERT_EQUAL(retval, 0);

   tear_down();
}

void test_lpd8806_map_output_to_point_one(void)
{
   set_up();

   int output = 1; 
   int x = -1;
   int y = -1;
   int retval = ambitv_lpd8806_map_output_to_point(SINK, output, DISPLAY_WIDTH, DISPLAY_HEIGHT, &x, &y);
   CU_ASSERT_EQUAL(x, 11);
   CU_ASSERT_EQUAL(y, 0);
   CU_ASSERT_EQUAL(retval, 0);

   tear_down();
}

void test_lpd8806_map_output_to_point_two(void)
{
   set_up();

   int output = 2; 
   int x = -1;
   int y = -1;
   int retval = ambitv_lpd8806_map_output_to_point(SINK, output, DISPLAY_WIDTH, DISPLAY_HEIGHT, &x, &y);
   CU_ASSERT_EQUAL(x, 22);
   CU_ASSERT_EQUAL(y, 0);
   CU_ASSERT_EQUAL(retval, 0);

   tear_down();
}

void test_lpd8806_map_output_to_point_last_in_first_line(void)
{
   set_up();

   int output = 9;
   int x = -1;
   int y = -1;
   int retval = ambitv_lpd8806_map_output_to_point(SINK, output, DISPLAY_WIDTH, DISPLAY_HEIGHT, &x, &y);
   CU_ASSERT_EQUAL(x, 100);
   CU_ASSERT_EQUAL(y, 0);
   CU_ASSERT_EQUAL(retval, 0);

   tear_down();
}

void test_lpd8806_map_output_to_point_second_last_in_first_line(void) {
   set_up();

   int output = 8;
   int x = -1;
   int y = -1;
   int retval = ambitv_lpd8806_map_output_to_point(SINK, output, DISPLAY_WIDTH, DISPLAY_HEIGHT, &x, &y);
   CU_ASSERT_EQUAL(x, 88);
   CU_ASSERT_EQUAL(y, 0);
   CU_ASSERT_EQUAL(retval, 0);

   tear_down();
}

void test_lpd8806_map_output_to_point_second_one_in_second_line(void) {
   set_up();

   int output = 11;
   int x = -1;
   int y = -1;
   int retval = ambitv_lpd8806_map_output_to_point(SINK, output, DISPLAY_WIDTH, DISPLAY_HEIGHT, &x, &y);
//   CU_ASSERT_EQUAL(x, 11);
//   CU_ASSERT_EQUAL(y, 11);
//   CU_ASSERT_EQUAL(retval, 0);

   tear_down();
}

int lpd8806_spidev_sink_test_add_suite() {
   CU_pSuite pSuite = NULL;

   /* add a suite to the registry */
   pSuite = CU_add_suite("lpd8806 spidev sink", &init_suite, &clean_suite);
   if (NULL == pSuite) {
      CU_cleanup_registry();
      return CU_get_error();
   }

   /* add the tests to the suite */
   if ((NULL == CU_add_test(pSuite, "test_lpd8806_map_output_to_point_zero", test_lpd8806_map_output_to_point_zero)) ||
       (NULL == CU_add_test(pSuite, "test_lpd8806_map_output_to_point_one", test_lpd8806_map_output_to_point_one)) ||
       (NULL == CU_add_test(pSuite, "test_lpd8806_map_output_to_point_two", test_lpd8806_map_output_to_point_two)) ||
       (NULL == CU_add_test(pSuite, "test_lpd8806_map_output_to_point_last_in_first_line", test_lpd8806_map_output_to_point_last_in_first_line)) ||
       (NULL == CU_add_test(pSuite, "test_lpd8806_map_output_to_point_second_last_in_first_line", test_lpd8806_map_output_to_point_second_last_in_first_line)) ||
       (NULL == CU_add_test(pSuite, "test_lpd8806_map_output_to_point_second_one_in_second_line", test_lpd8806_map_output_to_point_second_one_in_second_line))
      ) {
      CU_cleanup_registry();
      return CU_get_error();
   }
   return CU_get_error();
}
