
#ifndef __AMBITV_VIDEO_FMT_R__
#define __AMBITV_VIDEO_FMT_R__

void yuv_to_rgb(int y, int u, int v, unsigned char* r, unsigned char* g, unsigned char* b);
int avg_rgb_for_block_yuyv(unsigned char* rgb, const void* pixbuf, int x, int y, int w, int h, int bytesperline, int coarseness);


#endif // __AMBITV_VIDEO_FMT_R__
