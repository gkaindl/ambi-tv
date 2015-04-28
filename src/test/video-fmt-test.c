
#include "CUnit/Basic.h"

#include "video-fmt.h"
#include "video-fmt.r"

// Colorspace JPEG (V4L2_COLORSPACE_JPEG)
// Y' is scaled to [0...255] and Cb/Cr are scaled to [-128...128] and then clipped to [-128...127].
void test_video_fmt_yuv_to_rgb_black() {
  unsigned char rgb[3];

  yuv_to_rgb(16, 128, 128, &rgb[0], &rgb[1], &rgb[2]);
  
  CU_ASSERT_EQUAL(rgb[0], 0);
  CU_ASSERT_EQUAL(rgb[1], 0);
  CU_ASSERT_EQUAL(rgb[2], 0);
}

void test_video_fmt_yuv_to_rgb_white() {
  unsigned char rgb[3];

  yuv_to_rgb(255, 128, 128, &rgb[0], &rgb[1], &rgb[2]);

  CU_ASSERT_EQUAL(rgb[0], 255);
  CU_ASSERT_EQUAL(rgb[1], 255);
  CU_ASSERT_EQUAL(rgb[2], 255);
}

void test_video_fmt_yuv_to_rgb_blue() {
  unsigned char rgb[3];

  yuv_to_rgb(41, 240, 110, &rgb[0], &rgb[1], &rgb[2]);

  CU_ASSERT_EQUAL(rgb[0], 0);
  CU_ASSERT_EQUAL(rgb[1], 0);
  CU_ASSERT_EQUAL(rgb[2], 255);
}

void prgb(const char* msg, unsigned char *rgb) {
  printf("\n%s: RGB[%u,%u,%u]\n", msg, (unsigned int)rgb[0], (unsigned int)rgb[1], (unsigned int)rgb[2]);
}

void copy_range(void* dst, void* src, unsigned int dst_size, unsigned int src_size) {
  void* ptr;
  for (ptr = dst; ptr <= dst + (dst_size - src_size); ptr += src_size) {
    memcpy(ptr, src, src_size);
  }
}

void test_video_fmt_avg_rgb_for_block_yuyv() {
  unsigned char rgb[3];
  // 16 by 4 pixels -> 64 pixels * 2 bytes per pixel = 128
  unsigned char pixbuf[128] = { 0 };

  unsigned char yuyu[] = {16, 128, 16, 128};
  unsigned int offset = 0;
  unsigned int width = 4;
  copy_range(pixbuf, yuyu, 128, 4);

  int x = 0;
  int y = 0;
  int w = 4;
  int h = 4;
  int bytesperline = 16 * 2;
  int coarseness = 1;
  avg_rgb_for_block_yuyv(rgb, pixbuf, x, y, w, h, bytesperline, coarseness);
  prgb("avg", rgb);
  CU_ASSERT_EQUAL(rgb[0], 0);
  CU_ASSERT_EQUAL(rgb[1], 0);
  CU_ASSERT_EQUAL(rgb[2], 0);
}

int video_fmt_test_add_suite() {
   CU_pSuite pSuite = NULL;

   /* add a suite to the registry */
   pSuite = CU_add_suite("video-fmt", NULL, NULL);
   if (NULL == pSuite) {
      CU_cleanup_registry();
      return CU_get_error();
   }

   /* add the tests to the suite */
   if ((NULL == CU_add_test(pSuite, "test_video_fmt_avg_rgb_for_block_yuyv", test_video_fmt_avg_rgb_for_block_yuyv)) ||
       (NULL == CU_add_test(pSuite, "test_video_fmt_yuv_to_rgb_black", test_video_fmt_yuv_to_rgb_black)) ||
       (NULL == CU_add_test(pSuite, "test_video_fmt_yuv_to_rgb_white", test_video_fmt_yuv_to_rgb_white)) ||
       (NULL == CU_add_test(pSuite, "test_video_fmt_yuv_to_rgb_blue", test_video_fmt_yuv_to_rgb_blue)) ||
         0
      ) {
      CU_cleanup_registry();
      return CU_get_error();
   }
   return CU_get_error();
}
