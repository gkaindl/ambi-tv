
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
static const int _LED_STR_TOP[]    = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
static const int _LED_STR_BOTTOM[] = { 29, 28, 27, 26, 25, 24, 23, 22, 21, 20 };
static const int _LED_STR_LEFT[]   = { 39, 38, 37, 36, 35, 34, 33, 32, 31, 30 };
static const int _LED_STR_RIGHT[]  = { 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 };
static const int *_LED_STR[] = { 
   _LED_STR_TOP,
   _LED_STR_BOTTOM,
   _LED_STR_LEFT,
   _LED_STR_RIGHT
};
static const double _LED_INSET[] = { 0., 0., 0., 0.};

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

void set_up()
{
   set_up_sink();
}

void tear_down()
{
   tear_down_sink();
}

inline int *get_led_str(struct ambitv_sink_component* sink, int side, int idx)
{
   return &(((struct ambitv_lpd8806_priv*)sink->priv)->led_str[side][idx]);
}

inline double *get_inset(struct ambitv_sink_component* sink)
{
   return ((struct ambitv_lpd8806_priv*)sink->priv)->led_inset;
}

void test_lpd8806_ptr_for_output_top_right(void)
{
   set_up();

   int output = 9;
   int str_idx = -1;
   int led_idx = -1;

   int *ptr = ambitv_lpd8806_ptr_for_output(SINK->priv, output, &str_idx, &led_idx);
   CU_ASSERT_EQUAL(str_idx, 0);
   CU_ASSERT_EQUAL(led_idx, 9);
   CU_ASSERT_PTR_NOT_NULL_FATAL(ptr);
   CU_ASSERT_EQUAL(ptr, get_led_str(SINK, 0, 9));

   tear_down();
}

void test_lpd8806_ptr_for_output_top_left(void)
{
   set_up();

   int output = 0;
   int str_idx = -1;
   int led_idx = -1;

   int *ptr = ambitv_lpd8806_ptr_for_output(SINK->priv, output, &str_idx, &led_idx);
   CU_ASSERT_EQUAL(str_idx, 0);
   CU_ASSERT_EQUAL(led_idx, 0);
   CU_ASSERT_PTR_NOT_NULL_FATAL(ptr);
   CU_ASSERT_EQUAL(ptr, get_led_str(SINK, 0, 0));

   tear_down();
}

void test_lpd8806_ptr_for_output_left_top(void)
{
   set_up();

   // LEDs are indexed continuously through the side arrays top-bottom-left-right
   // not according to the stripe layout or clockwise sequence
   // The first LED on the left side comes after ten LEDs from the top, then ten LEDs
   // from the bottom; therefore it is number 20.
   int output = 20;
   int str_idx = -1;
   int led_idx = -1;

   int *ptr = ambitv_lpd8806_ptr_for_output(SINK->priv, output, &str_idx, &led_idx);
   CU_ASSERT_EQUAL(str_idx, 2);
   CU_ASSERT_EQUAL(led_idx, 0);
   CU_ASSERT_PTR_NOT_NULL_FATAL(ptr);
   CU_ASSERT_EQUAL(ptr, get_led_str(SINK, 2, 0));

   tear_down();
}

void test_lpd8806_ptr_for_output_bottom_left(void)
{
   set_up();

   // LEDs are indexed continuously through the side arrays top-bottom-left-right
   // not according to the stripe layout or clockwise sequence so ten means it is on
   // the second side which is the bottom one
   int output = 10;
   int str_idx = -1;
   int led_idx = -1;

   int *ptr = ambitv_lpd8806_ptr_for_output(SINK->priv, output, &str_idx, &led_idx);
   CU_ASSERT_EQUAL(str_idx, 1);
   CU_ASSERT_EQUAL(led_idx, 0);
   CU_ASSERT_PTR_NOT_NULL_FATAL(ptr);
   CU_ASSERT_EQUAL(ptr, get_led_str(SINK, 1, 0));

   tear_down();
}

void test_lpd8806_ptr_for_output_right_top(void)
{
   set_up();

   int output = 30;
   int str_idx = -1;
   int led_idx = -1;

   int *ptr = ambitv_lpd8806_ptr_for_output(SINK->priv, output, &str_idx, &led_idx);
   CU_ASSERT_EQUAL(str_idx, 3);
   CU_ASSERT_EQUAL(led_idx, 0);
   CU_ASSERT_PTR_NOT_NULL_FATAL(ptr);
   CU_ASSERT_EQUAL(ptr, get_led_str(SINK, 3, 0));

   tear_down();
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

void test_lpd8806_map_output_to_point_one_pal(void)
{
   set_up();
   int DISPLAY_WIDTH_PAL = 702;
   int DISPLAY_HEIGHT_PAL = 576;

   int output = 1; 
   int x = -1;
   int y = -1;
   int retval = ambitv_lpd8806_map_output_to_point(SINK, output, DISPLAY_WIDTH_PAL, DISPLAY_HEIGHT_PAL, &x, &y);
   CU_ASSERT_EQUAL(x, 78);
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

void test_lpd8806_map_output_to_point_last_on_the_top(void)
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

void test_lpd8806_map_output_to_point_second_last_on_the_top(void) {
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

void test_lpd8806_map_output_to_point_left_top(void) {
   set_up();

   // First LED on the third side means output: 20 ( 10 * (SIDE - 1) + (LED_IDX - 1) )
   int output = 20;
   int x = -1;
   int y = -1;
   int retval = ambitv_lpd8806_map_output_to_point(SINK, output, DISPLAY_WIDTH, DISPLAY_HEIGHT, &x, &y);
   CU_ASSERT_EQUAL(x, 0);
   CU_ASSERT_EQUAL(y, 0);
   CU_ASSERT_EQUAL(retval, 0);

   tear_down();
}

void test_lpd8806_map_output_to_point_left_bottom(void) {
   set_up();

   int output = 29;
   int x = -1;
   int y = -1;
   int retval = ambitv_lpd8806_map_output_to_point(SINK, output, DISPLAY_WIDTH, DISPLAY_HEIGHT, &x, &y);
   CU_ASSERT_EQUAL(x, 0);
   CU_ASSERT_EQUAL(y, 100);
   CU_ASSERT_EQUAL(retval, 0);

   tear_down();
}

void test_lpd8806_map_output_to_point_right_bottom(void) {
   set_up();

   int output = 39;
   int x = -1;
   int y = -1;
   int retval = ambitv_lpd8806_map_output_to_point(SINK, output, DISPLAY_WIDTH, DISPLAY_HEIGHT, &x, &y);
   CU_ASSERT_EQUAL(x, 100);
   CU_ASSERT_EQUAL(y, 100);
   CU_ASSERT_EQUAL(retval, 0);

   tear_down();
}

void test_lpd8806_map_output_to_point_second_led_on_the_right(void) {
   set_up();

   int output = 31;
   int x = -1;
   int y = -1;
   int retval = ambitv_lpd8806_map_output_to_point(SINK, output, DISPLAY_WIDTH, DISPLAY_HEIGHT, &x, &y);
   CU_ASSERT_EQUAL(x, 100);
   CU_ASSERT_EQUAL(y, 11);
   CU_ASSERT_EQUAL(retval, 0);

   tear_down();
}

void test_lpd8806_map_output_to_point_zero_with_inset(void)
{
   set_up();

   int output = 0; 
   int x = -1;
   int y = -1;
   double *inset = get_inset(SINK);
   inset[0] = -0.10;
   int retval = ambitv_lpd8806_map_output_to_point(SINK, output, DISPLAY_WIDTH, DISPLAY_HEIGHT, &x, &y);
   CU_ASSERT_EQUAL(x, 0);
   CU_ASSERT_EQUAL(y, 0);
   CU_ASSERT_EQUAL(retval, 0);

   tear_down();
}

void test_lpd8806_map_output_to_point_one_with_inset(void)
{
   set_up();

   int output = 1; 
   int x = -1;
   int y = -1;
   double *inset = get_inset(SINK);
   inset[0] = -0.10;
   int retval = ambitv_lpd8806_map_output_to_point(SINK, output, DISPLAY_WIDTH, DISPLAY_HEIGHT, &x, &y);
   CU_ASSERT_EQUAL(x, 3);
   CU_ASSERT_EQUAL(y, 0);
   CU_ASSERT_EQUAL(retval, 0);

   tear_down();
}

void test_lpd8806_map_output_to_point_top_second_last_with_inset(void)
{
   set_up();

   int output = 8; 
   int x = -1;
   int y = -1;
   double *inset = get_inset(SINK);
   inset[0] = -0.10;
   int retval = ambitv_lpd8806_map_output_to_point(SINK, output, DISPLAY_WIDTH, DISPLAY_HEIGHT, &x, &y);
   CU_ASSERT_EQUAL(x, 96);
   CU_ASSERT_EQUAL(y, 0);
   CU_ASSERT_EQUAL(retval, 0);

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
   if ((NULL == CU_add_test(pSuite, "test_lpd8806_ptr_for_output_top_left", test_lpd8806_ptr_for_output_top_left)) ||
       (NULL == CU_add_test(pSuite, "test_lpd8806_ptr_for_output_bottom_left", test_lpd8806_ptr_for_output_bottom_left)) ||
       (NULL == CU_add_test(pSuite, "test_lpd8806_ptr_for_output_top_right", test_lpd8806_ptr_for_output_top_right)) ||
       (NULL == CU_add_test(pSuite, "test_lpd8806_ptr_for_output_right_top", test_lpd8806_ptr_for_output_right_top)) ||
       (NULL == CU_add_test(pSuite, "test_lpd8806_ptr_for_output_left_top", test_lpd8806_ptr_for_output_left_top)) ||

       (NULL == CU_add_test(pSuite, "test_lpd8806_map_output_to_point_zero", test_lpd8806_map_output_to_point_zero)) ||
       (NULL == CU_add_test(pSuite, "test_lpd8806_map_output_to_point_one", test_lpd8806_map_output_to_point_one)) ||
       (NULL == CU_add_test(pSuite, "test_lpd8806_map_output_to_point_one_pal", test_lpd8806_map_output_to_point_one_pal)) ||
       (NULL == CU_add_test(pSuite, "test_lpd8806_map_output_to_point_two", test_lpd8806_map_output_to_point_two)) ||
       (NULL == CU_add_test(pSuite, "test_lpd8806_map_output_to_point_last_on_the_top", test_lpd8806_map_output_to_point_last_on_the_top)) ||
       (NULL == CU_add_test(pSuite, "test_lpd8806_map_output_to_point_second_last_on_the_top", test_lpd8806_map_output_to_point_second_last_on_the_top)) ||

       (NULL == CU_add_test(pSuite, "test_lpd8806_map_output_to_point_left_top", test_lpd8806_map_output_to_point_left_top)) ||
       (NULL == CU_add_test(pSuite, "test_lpd8806_map_output_to_point_left_bottom", test_lpd8806_map_output_to_point_left_bottom)) ||
       (NULL == CU_add_test(pSuite, "test_lpd8806_map_output_to_point_right_bottom", test_lpd8806_map_output_to_point_right_bottom)) ||
       (NULL == CU_add_test(pSuite, "test_lpd8806_map_output_to_point_second_led_on_the_right", test_lpd8806_map_output_to_point_second_led_on_the_right)) ||

       (NULL == CU_add_test(pSuite, "test_lpd8806_map_output_to_point_zero_with_inset", test_lpd8806_map_output_to_point_zero_with_inset)) ||
       (NULL == CU_add_test(pSuite, "test_lpd8806_map_output_to_point_one_with_inset", test_lpd8806_map_output_to_point_one_with_inset)) ||
       (NULL == CU_add_test(pSuite, "test_lpd8806_map_output_to_point_top_second_last_with_inset", test_lpd8806_map_output_to_point_top_second_last_with_inset))
       // TODO test for the coordinates why 11 and 22?
      ) {
      CU_cleanup_registry();
      return CU_get_error();
   }
   return CU_get_error();
}
