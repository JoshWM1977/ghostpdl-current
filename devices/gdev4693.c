/*
 *	Copyright 1992 Washington State University. All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted.
 * This software is provided "as is" without express or implied warranty.
 */

/* Driver for the Tektronix 4693d color plotter. */
#include "gdevprn.h"
#define prn_dev ((gx_device_printer *)dev) /* needed in 5.31 et seq */

/* Thanks to Karl Hakimian (hakimian@yoda.eecs.wsu.edu) */
/* for contributing this code to Aladdin Enterprises. */

#define X_DPI 100
#define Y_DPI 100
#define WIDTH_10THS 85
#define HEIGHT_10THS 110

static dev_proc_print_page(t4693d_print_page);
static dev_proc_map_rgb_color(gdev_t4693d_map_rgb_color);
static dev_proc_map_color_rgb(gdev_t4693d_map_color_rgb);

static void
t4693d_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs(dev);

    set_dev_proc(dev, output_page, gdev_prn_bg_output_page);
    set_dev_proc(dev, map_rgb_color, gdev_t4693d_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, gdev_t4693d_map_color_rgb);
    set_dev_proc(dev, encode_color, gdev_t4693d_map_rgb_color);
    set_dev_proc(dev, decode_color, gdev_t4693d_map_color_rgb);
}

/* Since the print_page doesn't alter the device, this device can print in the background */
#define t4693d_prn_device(name,depth,max_rgb) {prn_device_body( \
        gx_device_printer,t4693d_initialize_device_procs,name, \
        WIDTH_10THS, HEIGHT_10THS, X_DPI, Y_DPI, 0.25, 0.25, 0.25, 0.25, \
        3,depth,max_rgb,max_rgb,max_rgb + 1,max_rgb + 1, \
        t4693d_print_page)}

const gx_device_printer gs_t4693d2_device = t4693d_prn_device("t4693d2",8, 3);
const gx_device_printer gs_t4693d4_device = t4693d_prn_device("t4693d4",16, 15);
const gx_device_printer gs_t4693d8_device = t4693d_prn_device("t4693d8",24, 255);

static gx_color_index
gdev_t4693d_map_rgb_color(gx_device *dev, const gx_color_value cv[])
{
        ushort bitspercolor = prn_dev->color_info.depth / 3;
        ulong max_value = (1 << bitspercolor) - 1;

        gx_color_value r, g, b;
        r = cv[0]; g = cv[1]; b = cv[2];

        if (bitspercolor == 5) {
                bitspercolor--;
                max_value = (1 << bitspercolor) - 1;
        }

        return ((r*max_value/gx_max_color_value) << (bitspercolor*2)) +
                ((g*max_value/gx_max_color_value) << bitspercolor) +
                (b*max_value/gx_max_color_value);
}

static int
gdev_t4693d_map_color_rgb(gx_device *dev, gx_color_index color, ushort prgb[3])
{
        ushort bitspercolor = prn_dev->color_info.depth / 3;
        ulong max_value = (1 << bitspercolor) - 1;

        if (bitspercolor == 5) {
                bitspercolor--;
                max_value = (1 << bitspercolor) - 1;
        }

        prgb[0] = (color >> (bitspercolor*2)) * gx_max_color_value / max_value;
        prgb[1] = ((color >> bitspercolor) & max_value) * gx_max_color_value / max_value;
        prgb[2] = (color & max_value) * gx_max_color_value / max_value;
        return(0);
}

static int
t4693d_print_page(gx_device_printer *dev, gp_file *ps_stream)
{
        char header[32];
        int depth = prn_dev->color_info.depth;
        int line_size = gdev_mem_bytes_per_scan_line(prn_dev);
        byte *data = (byte *)gs_malloc(dev->memory, line_size, 1, "t4693d_print_page");
        char *p;
        ushort data_size = line_size/prn_dev->width;
        int checksum;
        int lnum, code = 0;
        int i;
#if !ARCH_IS_BIG_ENDIAN
        byte swap;
#endif

        if (data == 0) return_error(gs_error_VMerror);
        /* build header. */
        p = header;
        *p++ = (char)0x14;	/* Print request */
        *p++ = (char)0xc0|20;	/* Length of header */
        *p++ = (char)0xc0 | ((prn_dev->width >> 6)&0x3f);
        *p++ = (char)0x80 | (prn_dev->width&0x3f);
        *p++ = (char)0xc0 | ((prn_dev->height >> 6)&0x3f);
        *p++ = (char)0x80 | (prn_dev->height&0x3f);
        *p++ = (char)0xc1;	/* Handshake */
        *p++ = (char)0xc0;	/* Get number of prints from printer. */
        *p++ = (char)0xc0;	/* Get pixel shape from printer. */
        *p++ = (char)(depth == 8) ? 0xcb : (depth == 16) ? 0xcc : 0xcd;
        *p++ = (char)0xc1;	/* Pixel-data order 1. */
        *p++ = (char)0xc3;	/* Interpolate to maximum size. */
        *p++ = (char)0xc3;	/* Full color range 1. */
        *p++ = (char)0xc0;	/* Color conversion from printer. */
        *p++ = (char)0xc0;	/* Color manipulation from printer. */
        *p++ = (char)0xc0;	/* B/W inversion from printer. */
        *p++ = (char)0xc3;	/* Portrait mode centered. */
        *p++ = (char)0xc9;	/* Use printer default for media and printing. */
        *p++ = (char)0x95;
        *p++ = (char)0x81;

        for (checksum = 0, i = 0; &header[i] != p; i++)
                checksum += header[i];

        *p++ = ((checksum%128)&0x7f) | 0x80;
        *p = 0x02; /* end of line. */
        /* write header */
        if (gp_fwrite(header,1,22,ps_stream) != 22) {
                errprintf(dev->memory, "Could not write header (t4693d).\n");
                code = gs_note_error(gs_error_ioerror);
                goto xit;
        }

        for (lnum = 0; lnum < prn_dev->height; lnum++) {
                code = gdev_prn_copy_scan_lines(prn_dev,lnum,data,line_size);
                if (code < 0)
                    goto xit;

                for (i = 0; i < line_size; i += data_size) {

                        switch (depth) {
                        case 8:
                                data[i] &= 0x3f;
                                break;
                        case 16:
#if ARCH_IS_BIG_ENDIAN
                                data[i] &= 0x0f;
#else
                                swap = data[i];
                                data[i] = data[i + 1]&0x0f;
                                data[i + 1] = swap;
#endif
                                break;
                        case 24:
                                break;
                        default:
                                errprintf(dev->memory,"Bad depth (%d) t4693d.\n",depth);
                                code = gs_note_error(gs_error_rangecheck);
                                goto xit;
                        }

                        if (gp_fwrite(&data[i],1,data_size,ps_stream) != data_size) {
                                errprintf(dev->memory,"Could not write pixel (t4693d).\n");
                                code = gs_note_error(gs_error_ioerror);
                                goto xit;
                        }

                }

                if (gp_fputc(0x02,ps_stream) != 0x02) {
                        errprintf(dev->memory,"Could not write EOL (t4693d).\n");
                        code = gs_note_error(gs_error_ioerror);
                        goto xit;
                }

        }

        if (gp_fputc(0x01,ps_stream) != 0x01) {
                errprintf(dev->memory,"Could not write EOT (t4693d).\n");
                code = gs_note_error(gs_error_ioerror);
                /* fall through to xit: */
        }

xit:
        gs_free(dev->memory, data, line_size, 1, "t4693d_print_page");
        return(code);
}
