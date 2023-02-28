#include "imageprocess.h"
#ifdef YUV2JPEG
 char clip_value(char value, int min, int max)
    {
        if (value < min)
        {
            return min;
        }
        if (value > max)
        {
            return max;
        }
        return value;
    }

    int YUV420_TO_RGB24(char *yuvBuf_y, char *yuvBuf_uv, int w, int h, char *rgbBuf)
    {
        int index_y, index_u, index_v;
        char y, u, v;
        char r, g, b;

        for (size_t i = 0; i < h; i++)
        {
            for (size_t j = 0; j < w; j++)
            {

                index_y = i * w + j;
                index_u = (i / 2) * w + j - j % 2;
                index_v = index_u + 1;

                y = yuvBuf_y[index_y];

                u = yuvBuf_uv[index_u];
                v = yuvBuf_uv[index_v];

                b = y + 1.772 * (u - 128);                         // B = Y +1.779*(U-128)
                g = y - 0.34413 * (u - 128) - 0.71414 * (v - 128); // G = Y-0.3455*(U-128)-0.7169*(V-128)
                r = y + 1.402 * (v - 128);                         // R = Y+1.4075*(V-128)

                *(rgbBuf++) = clip_value(r, 0, 255);
                *(rgbBuf++) = clip_value(g, 0, 255);
                *(rgbBuf++) = clip_value(b, 0, 255);
            }
        }

        return 1;
    }
    int rgb2jpg(char *jpg_file, char *pdata, int width, int height)
    {
        int depth = 3;
        JSAMPROW row_pointer[1];
        struct jpeg_compress_struct cinfo;
        struct jpeg_error_mgr jerr;
        FILE *outfile;

        if ((outfile = fopen(jpg_file, "wb")) == NULL)
        {
            return -1;
        }

        cinfo.err = jpeg_std_error(&jerr);
        jpeg_create_compress(&cinfo);
        jpeg_stdio_dest(&cinfo, outfile);

        cinfo.image_width = width;
        cinfo.image_height = height;
        cinfo.input_components = depth;
        cinfo.in_color_space = JCS_RGB;
        jpeg_set_defaults(&cinfo);

        jpeg_set_quality(&cinfo, 100, TRUE);
        jpeg_start_compress(&cinfo, TRUE);

        int row_stride = width * depth;
        while (cinfo.next_scanline < cinfo.image_height)
        {
            row_pointer[0] = (JSAMPROW)(pdata + cinfo.next_scanline * row_stride);
            jpeg_write_scanlines(&cinfo, row_pointer, 1);
        }

        jpeg_finish_compress(&cinfo);
        jpeg_destroy_compress(&cinfo);
        fclose(outfile);

        return 0;
    }
    #endif
