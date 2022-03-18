/* Copyright (C) 2001-2021 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/

/*
The big commit:
  - color
  - comments
X - checkerr macro and refactor code to lasterr
X - fix (remove) leading CRLF in device init
*/


/*
 * Apple DMP / Imagewriter driver
 *
 * This is a modification of Mark Wedel's Apple DMP and
 * Jonathan Luckey's Imagewriter II driver to
 * support the Imagewriter LQ's higher resolution (320x216):
 *      appledmp:  120dpi x  72dpi is still supported (yuck)
 *	iwlo:	   160dpi x  72dpi
 *	iwhi:	   160dpi x 144dpi
 *      iwlq:      320dpi x 216dpi
 *
 * This is also my first attempt to work with gs. I have not included the LQ's
 * ability to print in colour. Perhaps at a later date I will tackle that.
 *
 * BTW, to get your Imagewriter LQ serial printer to work with a PC, attach it
 * with a nullmodem serial cable.
 *
 * Scott Barker (barkers@cuug.ab.ca)
 */

/*
 * This is a modification of Mark Wedel's Apple DMP driver to
 * support 2 higher resolutions:
 *      appledmp:  120dpi x  72dpi is still supported (yuck)
 *	iwlo:	   160dpi x  72dpi
 *	iwhi:	   160dpi x 144dpi
 *
 * The Imagewriter II is a bit odd.  In pinfeed mode, it thinks its
 * First line is 1 inch from the top of the page. If you set the top
 * form so that it starts printing at the top of the page, and print
 * to near the bottom, it thinks it has run onto the next page and
 * the formfeed will skip a whole page.  As a work around, I reverse
 * the paper about a 1.5 inches at the end of the page before the
 * formfeed to make it think its on the 'right' page.  bah. hack!
 *
 * This is  my first attempt to work with gs, so your milage may vary
 *
 * Jonathan Luckey (luckey@rtfm.mlb.fl.us)
 */

/* This is a bare bones driver I developed for my apple Dot Matrix Printer.
 * This code originally was from the epson driver, but I removed a lot
 * of stuff that was not needed.
 *
 * The Dot Matrix Printer was a predecessor to the apple Imagewriter.  Its
 * main difference being that it was parallel.
 *
 * This code should work fine on Imagewriters, as they have a superset
 * of commands compared to the DMP printer.
 *
 * This driver does not produce the smalles output files possible.  To
 * do that, it should look through the output strings and find repeat
 * occurances of characters, and use the escape sequence that allows
 * printing repeat sequences.  However, as I see it, this the limiting
 * factor in printing is not transmission speed to the printer itself,
 * but rather, how fast the print head can move.  This is assuming the
 * printer is set up with a reasonable speed (9600 bps)
 *
 * WHAT THE CODE DOES AND DOES NOT DO:
 *
 * To print out images, it sets the printer for unidirection printing
 * and 15 cpi (120 dpi). IT sets line feed to 1/9 of an inch (72 dpi).
 * When finished, it sets things back to bidirection print, 1/8" line
 * feeds, and 12 cpi.  There does not appear to be a way to reset
 * things to initial values.
 *
 * This code does not set for 8 bit characters (which is required). It
 * also assumes that carriage return/newline is needed, and not just
 * carriage return.  These are all switch settings on the DMP, and
 * I have configured them for 8 bit data and cr only.
 *
 * You can search for the strings Init and Reset to find the strings
 * that set up the printer and clear things when finished, and change
 * them to meet your needs.
 *
 * Also, you need to make sure that the printer daemon (assuming unix)
 * doesn't change the data as it is being printed.  I have set my
 * printcap file (sunos 4.1.1) with the string:
 * ms=pass8,-opost
 * and it works fine.
 *
 * Feel free to improve this code if you want.  However, please make
 * sure that the old DMP will still be supported by any changes.  This
 * may mean making an imagewriter device, and just copying this file
 * to something like gdevimage.c.
 *
 * The limiting factor of the DMP is the vertical resolution.  However, I
 * see no way to do anything about this.  Horizontal resolution could
 * be increased by using 17 cpi (136 dpi).  I believe the Imagewriter
 * supports 24 cpi (192 dpi).  However, the higher dpi, the slower
 * the printing.
 *
 * Dot Matrix Code by Mark Wedel (master@cats.ucsc.edu)
 *
 *
 * As of Oct 2019, maintained by Mike Galatean (contact through https://bugs.ghostscript.com )
 *
 */

#include "gdevprn.h"
#include "std.h"
#include "string.h"

 /* Device type macros */
#define DMP 1
#define IWLO 2
#define IWHI 3
#define IWLQ 4

/* Device resolution macros */
#define H120V72 1
#define H160V72 2
#define H160V144 3
#define H320V216 4

/* Device rendering modes */
#define GDEVPBMPLUS 2 /* gdev_prn_get_lines */
#define GDEVPBM 1 /* gdev_prn_get_lines */
#define GDEVADMP 0 /* gdev_prn_copy_scan_lines */

/* Color coding modes */
#define KENPKSMRAWPPM 0 //I think I might want PKSMRAW - 4 PBM pages

/* Device command macros */
#define ESC "\033"
#define CR "\r"
#define LF "\n"
#define FF "\f"

#define ELITE "E"
#define ELITEPROPORTIONAL "P"
#define CONDENSED "q"

#define LQPROPORTIONAL "a3"

#define BIDIRECTIONAL "<"
#define UNIDIRECTIONAL ">"

#define LINEFEEDFWD "f"
#define LINEFEEDREV "r"

#define LINEHEIGHT "T"
#define LINE8PI "B"

#define BITMAPLO "G"
#define BITMAPLORPT "V"
#define BITMAPHI "C"
#define BITMAPHIRPT "U"

#define COLORSELECT "K"
#define YELLOW "1"      /* (in the recommended order of printing for */
#define CYAN "3"       /* minimizing cross-band color contamination) */
#define MAGENTA "2"
#define BLACK "0"

#define DRIVERNAME "gdevadmp"
#define ERRALLOC DRIVERNAME ":Memory allocation failed.\n"
#define ERRDIRSELECT DRIVERNAME ": Printer initialization (directionality) failed.\n"
#define ERRREZSELECT DRIVERNAME ": Printer initialization (resolution) failed.\n"
#define ERRCOLORSELECT DRIVERNAME ": Printhead color positioning failed.\n"
#define ERRBLANKLINE DRIVERNAME ": Blank-line generation failed.\n"
#define ERRGETSCANLINE DRIVERNAME ": Error getting scanline from rendering engine.\n"
#define ERRNUMCOMP DRIVERNAME ": color_info.num_components out of range.\n"
#define ERRGPGL DRIVERNAME ": gdev_prn_get_lines() returned unexpected values.\n"
#define ERR DRIVERNAME ": .\n"

/* Error checking macro */
#define CHECKERR(call, expect, error)\
    lasterr = call;
    //if (lasterr expect)\
    //{\
    //    errprintf(pdev->memory, ERRBLANKLINE);
    //    lasterr = gs_note_error(error);\
    //    goto xit;\
    //} 

/* Device structure */
struct gx_device_admp_s {
        gx_device_common;
        gx_prn_device_common;
        bool color;
        bool unidirectional;
        int rendermode;
        int colorinfo;
        int colorcoding;
        bool debug;
};

typedef struct gx_device_admp_s gx_device_admp;

/* Device procedure initialization */
static dev_proc_put_params(admp_put_params);
static dev_proc_get_params(admp_get_params);
static dev_proc_print_page(admp_print_page);
//static dev_proc_encode_color(pgm_encode_color);
//static dev_proc_decode_color(ppm_decode_color);
static dev_proc_decode_color(ppm_decode_color);
static dev_proc_map_cmyk_color(pkm_map_cmyk_color);
static dev_proc_map_color_rgb(pkm_map_color_rgb);

/* Map a CMYK color to a pixel value. */
static gx_color_index
pkm_map_cmyk_color(gx_device* pdev, const gx_color_value cv[]) // from pkm_map_cmyk_color
{
        uint bpc = pdev->color_info.depth >> 2;
        uint max_value = pdev->color_info.max_color;
        uint cc = cv[0] * max_value / gx_max_color_value;
        uint mc = cv[1] * max_value / gx_max_color_value;
        uint yc = cv[2] * max_value / gx_max_color_value;
        uint kc = cv[3] * max_value / gx_max_color_value;
        gx_color_index color =
                ((((((gx_color_index)cc << bpc) + mc) << bpc) + yc) << bpc) + kc;

        return (color == gx_no_color_index ? color ^ 1 : color);
}


/* Map a CMYK pixel value to RGB. */
static int
pkm_map_color_rgb(gx_device* dev, gx_color_index color, gx_color_value rgb[3]) // from pkm_map_color_rgb
{
        int bpc = dev->color_info.depth >> 2;
        gx_color_index cshift = color;
        uint mask = (1 << bpc) - 1;
        uint k = cshift & mask;
        uint y = (cshift >>= bpc) & mask;
        uint m = (cshift >>= bpc) & mask;
        uint c = cshift >> bpc;
        uint max_value = dev->color_info.max_color;
        uint not_k = max_value - k;

#define CVALUE(c)\
  ((gx_color_value)((ulong)(c) * gx_max_color_value / max_value))
        /* We use our improved conversion rule.... */
        rgb[0] = CVALUE((max_value - c) * not_k / max_value);
        rgb[1] = CVALUE((max_value - m) * not_k / max_value);
        rgb[2] = CVALUE((max_value - y) * not_k / max_value);
#undef CVALUE
        return 0;
}

/* Map a PPM color tuple back to an RGB color. */
static int
ppm_decode_color(gx_device* dev, gx_color_index color,
        gx_color_value prgb[3])
{
        uint bitspercolor = dev->color_info.depth / 3;
        uint colormask = (1 << bitspercolor) - 1;
        uint max_rgb = dev->color_info.max_color;

        prgb[0] = ((color >> (bitspercolor * 2)) & colormask) *
                (ulong)gx_max_color_value / max_rgb;
        prgb[1] = ((color >> bitspercolor) & colormask) *
                (ulong)gx_max_color_value / max_rgb;
        prgb[2] = (color & colormask) *
                (ulong)gx_max_color_value / max_rgb;
        return 0;
}

static void
ppm_set_dev_procs(gx_device* pdev)
{
        gx_device_admp* const bdev = (gx_device_admp*)pdev;

        /*if (dev_proc(pdev, copy_alpha) != pnm_copy_alpha) {
                bdev->save_copy_alpha = dev_proc(pdev, copy_alpha);
                if (pdev->color_info.depth > 4)
                        set_dev_proc(pdev, copy_alpha, pnm_copy_alpha);
        }*/
        /*if (dev_proc(pdev, begin_typed_image) != pnm_begin_typed_image) {
                bdev->save_begin_typed_image = dev_proc(pdev, begin_typed_image);
                set_dev_proc(pdev, begin_typed_image, pnm_begin_typed_image);
        }*/
        if (bdev->color_info.num_components == 4) {
                if (bdev->color_info.depth == 4) {
                        set_dev_proc(pdev, map_color_rgb, cmyk_1bit_map_color_rgb);
                        set_dev_proc(pdev, map_cmyk_color, cmyk_1bit_map_cmyk_color);
                }
                /*else if (bdev->magic == '7') {
                        set_dev_proc(pdev, map_color_rgb, cmyk_8bit_map_color_rgb);
                        set_dev_proc(pdev, map_cmyk_color, cmyk_8bit_map_cmyk_color);
                }*/
                else {
                        set_dev_proc(pdev, map_color_rgb, pkm_map_color_rgb);
                        set_dev_proc(pdev, map_cmyk_color, pkm_map_cmyk_color);
                }
        }
}


static int
ppm_open(gx_device* pdev)
{
        gx_device_admp* bdev = (gx_device_admp*)pdev;
        int code;

#ifdef TEST_PAD_AND_ALIGN
        pdev->pad = 5;
        pdev->log2_align_mod = 6;
#endif

        code = gdev_prn_open_planar(pdev, bdev->color);
        while (pdev->child)
                pdev = pdev->child;

        bdev = (gx_device_admp*)pdev;;

        if (code < 0)
                return code;
        pdev->color_info.separable_and_linear = GX_CINFO_SEP_LIN;
        set_linear_color_bits_mask_shift(pdev);
        //bdev->uses_color = 0;
        ppm_set_dev_procs(pdev);
        return code;
}

static void
admp_initialize_device_procs(gx_device* pdev) //pk[sm]([m]?raw)?
{
        //outprintf(pdev->memory, "admp_initialize_device_procs()\n");

        /*
        set_dev_proc(pdev, map_rgb_color, NULL);
        set_dev_proc(pdev, map_color_rgb, NULL);
        set_dev_proc(pdev, map_cmyk_color, cmyk_1bit_map_cmyk_color);
        //set_dev_proc(pdev, decode_color, cmyk_1bit_map_color_cmyk);
        //set_dev_proc(pdev, encode_color, cmyk_1bit_map_cmyk_color);
        set_dev_proc(pdev, decode_color, iw_decode_color); // cmyk_1bit_map_color_cmyk
        set_dev_proc(pdev, encode_color, iw_encode_color); // cmyk_1bit_map_cmyk_color
        */

        /*
        outprintf(pdev->memory, "Color=%d\n", ((gx_device_admp*)pdev)->color);
        outprintf(pdev->memory, "RenderMode=%d\n", ((gx_device_admp*)pdev)->rendermode);
        outprintf(pdev->memory, "ColorInfo=%d\n", ((gx_device_admp*)pdev)->colorinfo);
        outprintf(pdev->memory, "Debug=%d\n", ((gx_device_admp*)pdev)->debug);
        */

        //outprintf(pdev->memory, "ColorCoding=%d:", ((gx_device_admp*)pdev)->colorcoding);
        switch (((gx_device_admp*)pdev)->colorcoding)
        {

        case KENPKSMRAWPPM:
                //outprintf(pdev->memory, "0\n", ((gx_device_admp*)pdev)->colorcoding);

                /* recursively expanded from pkm_initialize_device_procs in gdevpbm.c*/
                /* try #1 gdev_prn_initialize_device_procs_mono(pdev);
                set_dev_proc(pdev, map_rgb_color, NULL);
                set_dev_proc(pdev, decode_color, cmyk_1bit_map_color_rgb);
                set_dev_proc(pdev, encode_color, cmyk_1bit_map_cmyk_color);
                set_dev_proc(pdev, output_page, admp_print_page);
                */

                /* try #2*/
                /* pbm_initialize_device_procs */
                gdev_prn_initialize_device_procs_mono(pdev);

                set_dev_proc(pdev, encode_color, gx_default_b_w_mono_encode_color);
                set_dev_proc(pdev, decode_color, gx_default_b_w_mono_decode_color);
                //set_dev_proc(pdev, put_params, ppm_put_params);
                //set_dev_proc(pdev, output_page, ppm_output_page);

                /* ppm_initialize_device_procs */
                //set_dev_proc(pdev, get_params, ppm_get_params);
                set_dev_proc(pdev, map_rgb_color, gx_default_rgb_map_rgb_color);
                set_dev_proc(pdev, map_color_rgb, ppm_decode_color);
                set_dev_proc(pdev, encode_color, gx_default_rgb_map_rgb_color);
                set_dev_proc(pdev, decode_color, ppm_decode_color);
                set_dev_proc(pdev, open_device, ppm_open);

                /* pkm_initialize_device_procs */
                set_dev_proc(pdev, map_rgb_color, NULL);
                set_dev_proc(pdev, decode_color, cmyk_1bit_map_color_rgb);
                set_dev_proc(pdev, encode_color, cmyk_1bit_map_cmyk_color);

                //set_dev_proc(pdev, output_page, admp_print_page);
                break;
        default:
                //outprintf(pdev->memory, "-1\n", ((gx_device_admp*)pdev)->colorcoding);
                //set_dev_proc(pdev, output_page, admp_print_page);
                gdev_prn_initialize_device_procs(pdev);
        }

        set_dev_proc(pdev, get_params, admp_get_params);
        set_dev_proc(pdev, put_params, admp_put_params);
}

/* Standard DMP device descriptor */
gx_device_admp far_data gs_appledmp_device = {
prn_device_margins_body(gx_device_admp, admp_initialize_device_procs, "appledmp",	/* The print_page proc is compatible with allowing bg printing */
        DEFAULT_WIDTH_10THS_US_LETTER,  /* width_10ths, 8.5" */
        DEFAULT_HEIGHT_10THS_US_LETTER, /* height_10ths, 11" */
        120, 72,                        /* X_DPI, Y_DPI */
        0, 0, 0.25, 0.25, 0.25, 0.25,   /* origin and margins */
        1, 1, 1, 0, 2, 1,               /* maxcomp, depth, maxgray, maxcolor, numgray, numcolor */
        admp_print_page),
        0, 0 };

/*  lowrez ImageWriter device descriptor */
gx_device_admp far_data gs_iwlo_device = {
prn_device_margins_body(gx_device_admp, admp_initialize_device_procs, "iwlo",	/* The print_page proc is compatible with allowing bg printing */
        DEFAULT_WIDTH_10THS_US_LETTER,  /* width_10ths, 8.5" */
        DEFAULT_HEIGHT_10THS_US_LETTER, /* height_10ths, 11" */
        160, 72,                        /* X_DPI, Y_DPI */
        0, 0, 0.25, 0.25, 0.25, 0.25,   /* origin and margins */
        1, 1, 1, 0, 2, 1,               /* maxcomp, depth, maxgray, maxcolor, numgray, numcolor */
        admp_print_page),
        0, 0 };

/*  hirez ImageWriter device descriptor */
gx_device_admp far_data gs_iwhi_device = {
prn_device_margins_body(gx_device_admp, admp_initialize_device_procs, "iwhi",	/* The print_page proc is compatible with allowing bg printing */
        DEFAULT_WIDTH_10THS_US_LETTER,  /* width_10ths, 8.5" */
        DEFAULT_HEIGHT_10THS_US_LETTER, /* height_10ths, 11" */
        160, 144,                       /* X_DPI, Y_DPI */
        0, 0, 0.25, 0.25, 0.25, 0.25,   /* origin and margins */
        1, 1, 1, 0, 2, 1,               /* maxcomp, depth, maxgray, maxcolor, numgray, numcolor */
        admp_print_page),
        0, 0 };

/* LQ hirez ImageWriter device descriptor */
gx_device_admp far_data gs_iwlq_device = {
prn_device_margins_body(gx_device_admp, admp_initialize_device_procs, "iwlq",	/* The print_page proc is compatible with allowing bg printing */
        DEFAULT_WIDTH_10THS_US_LETTER,  /* width_10ths, 8.5" */
        DEFAULT_HEIGHT_10THS_US_LETTER, /* height_10ths, 11" */
        320, 216,                       /* X_DPI, Y_DPI */
        0, 0, 0, 0, 0, 0,               /* origin and margins */
// gx_device_color_info - gxdevcli.h:258
//      4, 4, 1, 4, 1, 4,              /* ncomp, depth, mg, mc, dg, dc - prints 4 identical (but longer?) lines */
//      4, 4, 1, 1, 2, 4,
//      4, 4, 1, 1, 2, 2,              //dci_macro(4,4)
//      4, 4, 1, 1, 1, 1,              /* ncomp, depth, mg, mc, dg, dc - prints blank lines */
//      4, 4, 0, 0, 0, 0,
//      4, 1, 2, 5, 1, 4,
//      4, 1, 1, 1, 1, 1,              /* ncomp, depth, mg, mc, dg, dc - prints blank lines */
//      4, 1, 1, 4, 1, 4,              /* ncomp, depth, mg, mc, dg, dc - prints 4 identical lines */
//      4, 1, 1, 4, 1, 4,              /* ncomp, depth, mg, mc, dg, dc - prints ? lines */
//      4, 1, 1, 4, 0, 4,              /* ncomp, depth, mg, mc, dg, dc - ? */
//      4, 1, 1, 4, 2, 1,
        4, 1, 1, 0, 2, 1,              //dci_macro(4,1)
//      4, 1, 0, 4, 1, 5,              /* ncomp, depth, mg, mc, dg, dc - prints 4 identical lines */
//      4, 1, 0, 4, 0, 4,              /* ncomp, depth, mg, mc, dg, dc - prints 4 identical lines */
//      4, 1, 0, 0, 0, 0,
//      3, 1, 1, 3, 2, 4,
//      1, 1, 1, 0, 2, 1,              //dci_macro(dci_black_and_white)
//      1, 1, 1, 0, 2, 1,              /* maxcomp, depth, maxgray, maxcolor, numgray, numcolor */
        admp_print_page),
        1, 0, 0, 2, 0, 0 }; /* colorbool, directionbool, renderingmode, colorinfo, colorcoding, debug */

static int
admp_get_params(gx_device* pdev, gs_param_list* plist)
{
        outprintf(pdev->memory, "admp_get_params()\n");

        int lasterr = gdev_prn_get_params(pdev, plist);

        if (lasterr < 0 ||
                (lasterr = param_write_bool(plist, "Color", &((gx_device_admp*)pdev)->color)) < 0 ||
                (lasterr = param_write_bool(plist, "Unidirectional", &((gx_device_admp*)pdev)->unidirectional)) < 0 ||
                (lasterr = param_write_int(plist, "RenderMode", &((gx_device_admp*)pdev)->rendermode)) < 0 ||
                (lasterr = param_write_int(plist, "ColorInfo", &((gx_device_admp*)pdev)->colorinfo)) < 0 ||
                (lasterr = param_write_int(plist, "ColorCoding", &((gx_device_admp*)pdev)->colorcoding)) < 0 ||
                (lasterr = param_write_bool(plist, "Debug", &((gx_device_admp*)pdev)->debug)) < 0
                )
                return lasterr;

        return lasterr;
}

static int
admp_put_params(gx_device* pdev, gs_param_list* plist)
{
        outprintf(pdev->memory, "admp_get_params()\n");

        pdev->color_info.separable_and_linear = GX_CINFO_SEP_LIN;
        set_linear_color_bits_mask_shift(pdev);

        int lasterr = 0;
        bool color = ((gx_device_admp*)pdev)->color;
        bool unidirectional = ((gx_device_admp*)pdev)->unidirectional;
        int rendermode = ((gx_device_admp*)pdev)->rendermode;
        int colorinfo = ((gx_device_admp*)pdev)->colorinfo;
        int colorcoding = ((gx_device_admp*)pdev)->colorcoding;
        bool debug = ((gx_device_admp*)pdev)->debug;
        gs_param_name param_name;
        int elasterr = 0;
        gx_device_color_info color_info_4bit = dci_std_color_(4, 4);
        gx_device_color_info color_info_1bit = dci_std_color_(4, 1);
        gx_device_color_info color_info_mono = dci_black_and_white;

        if ((lasterr = param_read_bool(plist,
                (param_name = "Unidirectional"),
                &unidirectional)) < 0) {
                param_signal_error(plist, param_name, elasterr = lasterr);
        }
        if (elasterr < 0)
                return lasterr;

        if ((lasterr = param_read_bool(plist,
                (param_name = "Color"),
                &color)) < 0) {
                param_signal_error(plist, param_name, elasterr = lasterr);
        }
        if (elasterr < 0)
                return lasterr;

        if ((lasterr = param_read_int(plist,
                (param_name = "RenderMode"),
                &rendermode)) < 0) {
                param_signal_error(plist, param_name, elasterr = lasterr);
        }
        if (elasterr < 0)
                return lasterr;

        if ((lasterr = param_read_int(plist,
                (param_name = "ColorInfo"),
                &colorinfo)) < 0) {
                param_signal_error(plist, param_name, elasterr = lasterr);
        }
        if (elasterr < 0)
                return lasterr;

        if ((lasterr = param_read_int(plist,
                (param_name = "ColorCoding"),
                &colorcoding)) < 0) {
                param_signal_error(plist, param_name, elasterr = lasterr);
        }
        if (elasterr < 0)
                return lasterr;

        if ((lasterr = param_read_bool(plist,
                (param_name = "Debug"),
                &debug)) < 0) {
                param_signal_error(plist, param_name, elasterr = lasterr);
        }
        if (elasterr < 0)
                return lasterr;
        
        ((gx_device_admp*)pdev)->color = color;
        ((gx_device_admp*)pdev)->unidirectional = unidirectional;
        ((gx_device_admp*)pdev)->rendermode = rendermode;
        ((gx_device_admp*)pdev)->colorinfo = colorinfo;
        ((gx_device_admp*)pdev)->colorcoding = colorcoding;
        ((gx_device_admp*)pdev)->debug = debug;
        
        /* try to reverse engineer and dynamically set color_info values */
        if (((gx_device_admp*)pdev)->color)
        {
                switch (((gx_device_admp*)pdev)->colorinfo)
                {
                case 2:
                        pdev->color_info = color_info_4bit;
                        break;
                case 1:
                        pdev->color_info = color_info_1bit;
                        break;
                default: pdev->color_info = color_info_mono;
                }

                //pdev->max_components = ;
                //pdev->num_components = ;
                //pdev->polarity = ;
                //pdev->depth = ;
                //pdev->gray_index = ;
                //pdev->max_gray = ;
                //pdev->max_color = ;
                //pdev->dither_grays = ;
                //pdev->dither_colors = ;
                //pdev->antialias = ;
                //...

                gdev_prn_put_params(pdev, plist);

                if (pdev->is_open)
                        return gs_closedevice(pdev);

                return 0;
        }
        else
                return gdev_prn_put_params(pdev, plist);
}

/* Send the page to the printer output file. */
static int
admp_print_page(gx_device_printer* pdev, gp_file* gprn_stream)
{
        outprintf(pdev->memory, "admp_print_page()\n");
        outprintf(pdev->memory, "Color=%d\n", ((gx_device_admp*)pdev)->color);
        outprintf(pdev->memory, "RenderMode=%d\n", ((gx_device_admp*)pdev)->rendermode);
        outprintf(pdev->memory, "ColorInfo=%d\n", ((gx_device_admp*)pdev)->colorinfo);
        outprintf(pdev->memory, "ColorCoding=%d\n", ((gx_device_admp*)pdev)->colorcoding);
        outprintf(pdev->memory, "Debug=%d\n", ((gx_device_admp*)pdev)->debug);
        int lasterr = gs_error_ok;
        char pcmd[255];
        int dev_rez;
        int dev_type;
        unsigned char plane;
        int line_size = bitmap_raster(pdev->width /** pdev->color_info.depth*/);
        int in_size = line_size * 8; /* Note that in_size is a multiple of 8 dots in height. */

        char plane_order[4];

        /* C doesn't seem to like conditional declarations very much,
           so declare everything here, even if we're not in color mode.
           -- JWM
        */
        gx_color_index yellow_mask, cyan_mask, magenta_mask, black_mask;
        gx_render_plane_t yellow_plane, cyan_plane, magenta_plane, black_plane;
        byte *bufY, *bufC, *bufM, *bufK, *bufT, *prn;
        int black_plane_num = 0;

        switch (pdev->color_info.num_components)
        {
        case 4:
                plane_order[0] = 1;
                plane_order[1] = 3;
                plane_order[2] = 2;
                plane_order[3] = 0;

                if (((gx_device_admp*)pdev)->rendermode == GDEVPBMPLUS)
                {
                        gx_render_plane_init(&yellow_plane, (gx_device*)pdev, 2);
                        gx_render_plane_init(&cyan_plane, (gx_device*)pdev, 0);
                        gx_render_plane_init(&magenta_plane, (gx_device*)pdev, 1);
                }

                bufY = (byte*)gs_malloc(pdev->memory, /*pdev->color_info.num_components **/ in_size, 1, "admp_print_page(bufY)");
                bufC = (byte*)gs_malloc(pdev->memory, /*pdev->color_info.num_components **/ in_size, 1, "admp_print_page(bufC)");
                bufM = (byte*)gs_malloc(pdev->memory, /*pdev->color_info.num_components **/ in_size, 1, "admp_print_page(bufM)");
                black_plane_num = 3;
        case 1:
                plane_order[0] = 0;
                plane_order[1] = 0;
                plane_order[2] = 0;
                plane_order[3] = 0;

                if (((gx_device_admp*)pdev)->rendermode == GDEVPBMPLUS)
                {
                        gx_render_plane_init(&black_plane, (gx_device*)pdev, black_plane_num);
                }
                else
                {
                        int black_plane = 0;
                }

                bufK = (byte*)gs_malloc(pdev->memory, /*pdev->color_info.num_components **/ in_size, 1, "admp_print_page(bufK)");
                bufT = (byte*)gs_malloc(pdev->memory, /*pdev->color_info.num_components **/ in_size, 1, "admp_print_page(bufT)");
                prn = (byte*)gs_malloc(pdev->memory, /*pdev->color_info.num_components **/ 3*in_size, 1, "admp_print_page(prn)");
                break;
        default:
                outprintf(pdev->memory, ERRNUMCOMP);
                gs_note_error(gs_error_rangecheck);
                goto xit;
        }

        byte *in = bufK;
        byte *out = bufT;
        int lnum = 0;

        /* Check allocations */
        if (    bufK == 0 ||
                bufT == 0 ||
                prn  == 0 ||
                (       pdev->color_info.num_components == 4 &&
                       (bufY == 0 ||
                        bufM == 0 ||
                        bufC == 0) ) )
        {
                errprintf(pdev->memory, ERRALLOC);
                lasterr = gs_note_error(gs_error_VMerror);
                goto xit;
        }

        /* Determine device type */
        if (strcmp(pdev->dname, "iwlq") == 0) dev_type = IWLQ;
        else if (strcmp(pdev->dname, "iwhi") == 0) dev_type = IWHI;
        else if (strcmp(pdev->dname, "iwlo") == 0) dev_type = IWLO;
        else if (strcmp(pdev->dname, "appledmp") == 0) dev_type = DMP;
        else
        {
                gs_note_error(gs_error_rangecheck);
                goto xit;
        }

        /* Set resolution */
        if (pdev->y_pixels_per_inch == 216) dev_rez = H320V216;
        else if (pdev->y_pixels_per_inch == 144) dev_rez = H160V144;
        else if (pdev->x_pixels_per_inch == 160) dev_rez = H160V72;
        else dev_rez = H120V72;

        /* Initialize the printer. */
        if (((gx_device_admp*)pdev)->unidirectional)
        {
                (void)strcpy(pcmd, ESC UNIDIRECTIONAL ESC LINEHEIGHT "16"); 
                lasterr = gp_fputs(pcmd, gprn_stream);
        }
        else
        {
                (void)strcpy(pcmd, ESC BIDIRECTIONAL ESC LINEHEIGHT "16");
                lasterr = gp_fputs(pcmd, gprn_stream);
        }

        if (lasterr != strlen(pcmd))
        {
                errprintf(pdev->memory, ERRDIRSELECT);
                lasterr = gs_note_error(gs_error_ioerror);
                goto xit;
        }

        /* Set character pitch */
        switch (dev_rez)
        {
        case H320V216:
                (void)strcpy(pcmd, ESC ELITEPROPORTIONAL ESC LQPROPORTIONAL);
                lasterr = gp_fputs(pcmd, gprn_stream);
                break;
        case H160V144:
        case H160V72:
                (void)strcpy(pcmd, ESC ELITEPROPORTIONAL);
                lasterr = gp_fputs(pcmd, gprn_stream);
                break;
        case H120V72:
        default:
                (void)strcpy(pcmd, ESC CONDENSED);
                lasterr = gp_fputs(pcmd, gprn_stream);
                break;
        }

        if (lasterr != strlen(pcmd))
        {
                errprintf(pdev->memory, ERRREZSELECT);
                lasterr = gs_note_error(gs_error_ioerror);
                goto xit;
        }

        /* Print lines of graphics */
        while ( lnum < pdev->height )
        {
                byte *inp;
                byte *in_end;
                byte *out_end;
                int lcnt,ltmp;
                int count, passes;
                byte *prn_blk, *prn_end, *prn_tmp;

                /* 8-dot tall bands per pass */
                switch (dev_rez)
                {
                        case H320V216: passes = 3; break;
                        case H160V144: passes = 2; break;
                        case H160V72:
                        case H120V72:
                        default: passes = 1; break;
                }

                /* more gdevpbm.c style...
                 * The following initialization is unnecessary: lnum == band_end on
                 * the first pass through the loop below, so marked will always be
                 * set before it is used.  We initialize marked solely to suppress
                 * bogus warning messages from certain compilers.
             
                  gx_color_index marked = 0;
                  int plane_depth;
                  int plane_shift;

                  plane_depth = render_plane.depth;
                  plane_shift = render_plane.shift;
                  plane_mask = ((gx_color_index)1 << plane_depth) - 1;
                */

                //for (plane = 0; plane < pdev->color_info.num_components; plane++)
                for (plane = 0; plane < 1; plane++)
                {
                        if (pdev->color_info.num_components == 4)
                        {
                                switch (plane_order[plane])
                                {
                                        case 3: (void)strcpy(pcmd, ESC COLORSELECT CYAN);
                                        case 2: (void)strcpy(pcmd, ESC COLORSELECT MAGENTA);
                                        case 1: (void)strcpy(pcmd, ESC COLORSELECT YELLOW);
                                        case 0: (void)strcpy(pcmd, ESC COLORSELECT BLACK);
                                        default:
                                                lasterr = gp_fputs(pcmd, gprn_stream);
                                        
                                                if (lasterr != strlen(pcmd))
                                                {
                                                        errprintf(pdev->memory, ERRCOLORSELECT);
                                                        lasterr = gs_note_error(gs_error_ioerror);
                                                        goto xit;
                                                }
                                }
                        }

                        /* get rendered scanlines and assemble into 8, 16, or 24-dot high strips */
                        for (count = 0; count < passes; count++)
                        {
                                for (lcnt = 0; lcnt < 8; lcnt++)
                                {
                                        switch (dev_rez)
                                        {
                                                case H320V216: ltmp = lcnt + 8 * count; break;
                                                case H160V144: ltmp = 2 * lcnt + count; break;
                                                case H160V72:
                                                case H120V72:
                                                default: ltmp = lcnt; break;
                                        }

                                        if ((lnum + ltmp) > pdev->height) /* if we're at end of page */
                                        {
                                                /* write a blank line */
                                                lasterr = (int)memset(in + lcnt * line_size, 0, line_size);
                                                if (lasterr != (int)(in + lcnt * line_size))
                                                {
                                                        errprintf(pdev->memory, ERRBLANKLINE);
                                                        lasterr = gs_note_error(gs_error_undefined);
                                                        goto xit;
                                                }
                                        }
                                        else /* otherwise, get scan lines */
                                        {
                                                if (((gx_device_admp*)pdev)->debug) outprintf(pdev->memory, "y: %4d, str: %8x, lnum: %4d, lcnt: %2d, ltmp: %2d, plane: %x\n", ((lnum * (plane + 1)) + ltmp), in + line_size * (7 - (lcnt * (plane + 1))), lnum, lcnt, ltmp, plane);

                                                
                                                switch (((gx_device_admp*)pdev)->rendermode)
                                                {
                                                case GDEVPBM:
                                                        /*
                                                        if (lnum == band_end) {
                                                                gx_color_usage_t color_usage;
                                                                int band_start;
                                                                int band_height =
                                                                        gdev_prn_color_usage((gx_device*)pdev, lnum, 1,
                                                                                &color_usage, &band_start);

                                                                band_end = band_start + band_height;
                                                                marked = color_usage. or &(plane_mask << plane_shift);
                                                                if (!marked)
                                                                        memset(data, 0, raster);
                                                        }
                                                        if (marked)
                                                        {
                                                        */
                                                                uint actual_line_size;

                                                                //black_plane.index = 0;

                                                                /* 
                                                                Read one or more rasterized scan lines for printing.

                                                                copies the scan lines into the buffer
                                                                sets *actual_buffer = buffer and *actual_bytes_per_line = bytes_per_line,
                                                                or
                                                                sets *actual_buffer and *actual_bytes_per_line to reference already-rasterized scan lines.
                                                                                                                *
                                                                For non-banded devices, copying is never necessary.

                                                                For banded devices, if the client's buffer is at least as large as the single preallocated
                                                                one (if any), the band will be rasterized directly into the client's
                                                                buffer, again avoiding copying.  Alternatively, if there is a
                                                                preallocated buffer and the call asks for no more data than will fit
                                                                in that buffer, buffer may be NULL: any necessary rasterizing will
                                                                occur in the preallocated buffer, and a pointer into the buffer will be
                                                                returned.

                                                                int gdev_prn_get_lines(gx_device_printer * pdev,
                                                                        int y,
                                                                        int height,
                                                                        byte * buffer,
                                                                        uint bytes_per_line,
                                                                        byte * *actual_buffer,
                                                                        uint * actual_bytes_per_line,
                                                                        const gx_render_plane_t * render_plane);
                                                                */

                                                                if (((gx_device_admp*)pdev)->debug) outprintf(pdev->memory, "pre-out: %d", out);
                                                                lasterr = (int)gdev_prn_get_lines(pdev,
                                                                        lnum + ltmp,
                                                                        1,
                                                                        out + line_size * (7 - lcnt),
                                                                        (uint)line_size,
                                                                        &out,
                                                                        &actual_line_size,
                                                                        &black_plane);
                                                                if (((gx_device_admp*)pdev)->debug) outprintf(pdev->memory, ", post-out: %d\n", out);

                                                                if (lasterr !=1 && actual_line_size != line_size )
                                                                {
                                                                        errprintf(pdev->memory, ERRGPGL);
                                                                        lasterr = gs_note_error(gs_error_rangecheck);
                                                                        goto xit;
                                                                }       
                                                        break;

                                                case GDEVADMP:
                                                        /* gdev_prn_copy_scan_lines(gx_device_printer * pdev, int y, byte * str, uint size) - gdevprn.c:1641 */
                                                        /* if (((gx_device_admp*)pdev)->debug && lnum > 64) goto xit; */
                                                        /* copy scanlines to the buffer */
                                                        lasterr = gdev_prn_copy_scan_lines(
                                                                pdev,
                                                                (lnum + ltmp) * (plane + 1),
                                                                /*((lnum*(plane+1)) + ltmp),*/
                                                                in + line_size * (7 - lcnt), /*( lcnt * (plane + 1))),*/
                                                                line_size);

                                                        if (lasterr != 1 && lasterr != 0) /*line_size)*/
                                                        {
                                                                errprintf(pdev->memory, "lasterr=%d,line_size=%d\n", lasterr, line_size);
                                                                errprintf(pdev->memory, ERRGETSCANLINE);
                                                                lasterr = gs_note_error(gs_error_rangecheck);
                                                                goto xit;
                                                        }
                                                        
                                                        break;
                                                }
                                        }
                                }

                                out_end = out;
                                
                                //gp_fflush(gprn_stream); /* flush the file here to guarantee output earlier in the pipeline while debugging */
                                                                
                                switch (((gx_device_admp*)pdev)->rendermode)
                                {
                                case GDEVPBM:
                                        break;
                                case GDEVADMP:
                                        inp =  in;
                                        in_end = inp + line_size;
                                        for (; inp < in_end; inp++, out_end += 8)
                                        {
                                               /* The apple DMP printer seems to be odd in that the bit order on
                                                * each line is reverse what might be expected.  Meaning, an
                                                * underscore would be done as a series of 0x80, while on overscore
                                                * would be done as a series of 0x01.  So we get each
                                                * scan line in reverse order. -- Unknown
                                                */
                                                /* gdev_prn_transpose(...) returns void */
                                                gdev_prn_transpose_8x8(inp, line_size, out_end, 1);

                                        }
                                        out_end = out;
                                        break;
                                }

                                switch (dev_rez)
                                {
                                case H320V216: prn_end = prn + count; break;
                                case H160V144: prn_end = prn + in_size * count; break;
                                case H160V72:
                                case H120V72:
                                default: prn_end = prn; break;
                                }

                                while ((int)(out_end - out) < in_size)
                                {
                                        *prn_end = *(out_end++);
                                        if (dev_rez == H320V216) prn_end += 3;
                                        else prn_end++;
                                }
                        }

                        /* send bitmaps to printer, based on resolution setting

                           note: each of the three vertical resolutions has a 
                           unique and separate implementation
                        */
                        switch (dev_rez)
                        {
                        case H320V216:
                                prn_blk = prn;
                                prn_end = prn_blk + in_size * 3;
                                /* determine right edge of bitmap data */
                                while (prn_end > prn && prn_end[-1] == 0 &&
                                        prn_end[-2] == 0 && prn_end[-3] == 0)
                                {
                                        prn_end -= 3;
                                }
                                /* determine left edge of bitmap data */
                                while (prn_blk < prn_end && prn_blk[0] == 0 &&
                                        prn_blk[1] == 0 && prn_blk[2] == 0)
                                {
                                        prn_blk += 3;
                                }
                                /* send bitmaps to printer, if any */
                                if (prn_end != prn_blk)
                                {
                                        if ((prn_blk - prn) > 7)
                                        {
                                                /* set print-head position by sending a repeated all-zeros hirez bitmap */
                                                lasterr = gp_fprintf(gprn_stream,
                                                        ESC BITMAPHIRPT "%04d%c%c%c",
                                                        (int)((prn_blk - prn) / 3),
                                                        0, 0, 0);
                                                if (lasterr != 9)
                                                {
                                                        errprintf(pdev->memory, "gdevadmp: Printer ??? failed.\n");
                                                        lasterr = gs_note_error(gs_error_ioerror);
                                                        goto xit;
                                                }
                                        }
                                        else
                                                prn_blk = prn;

                                        /* send hirez bitmapped graphics mode command with length parameter*/
                                        CHECKERR(gp_fprintf(gprn_stream,
                                                ESC BITMAPHI "%04d",
                                                (int)((prn_end - prn_blk) / 3)),
                                                != 6,
                                                gs_error_ioerror)
                                        
                                        /* send actual bitmap data */
                                        CHECKERR(gp_fwrite(prn_blk,
                                                1,
                                                (int)(prn_end - prn_blk),
                                                gprn_stream),
                                                != (int)(prn_end - prn_blk),
                                                gs_error_ioerror)
                                }
                                break;
                        case H160V144:
                                for (count = 0; count < 2; count++)
                                {
                                        prn_blk = prn_tmp = prn + in_size * count;
                                        prn_end = prn_blk + in_size;
                                        while (prn_end > prn_blk && prn_end[-1] == 0)
                                                prn_end--;
                                        while (prn_blk < prn_end && prn_blk[0] == 0)
                                                prn_blk++;
                                        if (prn_end != prn_blk)
                                        {
                                                if ((prn_blk - prn_tmp) > 7)
                                                {
                                                        CHECKERR(gp_fprintf(gprn_stream,
                                                                ESC BITMAPLORPT "%04d%c",
                                                                (int)(prn_blk - prn_tmp),
                                                                0),
                                                                != 7,
                                                                gs_error_ioerror)
                                                }
                                                else
                                                        prn_blk = prn_tmp;

                                                CHECKERR(gp_fprintf(gprn_stream,
                                                        ESC BITMAPLO "%04d",
                                                        (int)(prn_end - prn_blk)),
                                                        != 6,
                                                        gs_error_ioerror)

                                                CHECKERR(gp_fwrite(prn_blk,
                                                        1,
                                                        (int)(prn_end - prn_blk),
                                                        gprn_stream),
                                                        != (int)(prn_end - prn_blk),
                                                        gs_error_ioerror)
                                        }
                                        if (!count)
                                        {
                                                CHECKERR(gp_fputs(ESC LINEHEIGHT "01" CR LF,
                                                        gprn_stream),
                                                        != 6,
                                                        gs_error_ioerror)
                                        }
                                }
                                CHECKERR(gp_fputs(ESC LINEHEIGHT "15", gprn_stream),
                                        != 4,
                                        gs_error_ioerror)
                                        break;
                        case H160V72:
                        case H120V72:
                        default:
                                prn_blk = prn;
                                prn_end = prn_blk + in_size;
                                while (prn_end > prn_blk && prn_end[-1] == 0)
                                        prn_end--;
                                while (prn_blk < prn_end && prn_blk[0] == 0)
                                        prn_blk++;
                                if (prn_end != prn_blk)
                                {
                                        if ((prn_blk - prn) > 7)
                                        {
                                                CHECKERR(gp_fprintf(gprn_stream,
                                                        ESC BITMAPLORPT "%04d%c",
                                                        (int)(prn_blk - prn),
                                                        0),
                                                        != 7,
                                                        gs_error_ioerror)
                                        }
                                        else
                                                prn_blk = prn;

                                        CHECKERR(gp_fprintf(gprn_stream,
                                                ESC BITMAPLO "%04d",
                                                (int)(prn_end - prn_blk)),
                                                != 6,
                                                gs_error_ioerror)

                                        CHECKERR(gp_fwrite(prn_blk,
                                                1,
                                                (int)(prn_end - prn_blk),
                                                gprn_stream),
                                                != (int)(prn_end - prn_blk),
                                                gs_error_ioerror)
                                }
                                break;
                        }

                        /*switch on color or num comp ?
                        if (plane == 3)
                        {
                                CHECKERR(gp_fputs(CR LF, gprn_stream),
                                        != 2,
                                        gs_error_ioerror)
                        }
                        else
                        {
                                CHECKERR(gp_fputs(CR LF, gprn_stream),
                                        != 2,
                                        gs_error_ioerror)
                        }
                        */
                        CHECKERR(gp_fputs(CR LF, gprn_stream),
                                != 2,
                                gs_error_ioerror)
                }

                switch (dev_rez)
                        {
                        case H320V216: lnum += 24; break;
                        case H160V144: lnum += 16; break;
                        case H160V72:
                        case H120V72:
                        default: lnum += 8; break;
                        }
        }

        /* ImageWriter will skip a whole page if too close to end */
        /* so skip back more than an inch */
        /*
        if ( !(dev_type == DMP) )
        {
        CHECKERR(gp_fputs(ESC LINEHEIGHT "99" LF LF ESC LINEFEEDREV LF LF LF LF ESC LINEFEEDFWD,
                        gprn_stream),
                != 14,
                gs_error_ioerror)
        }
        */

        /* Formfeed and Reset printer */
        CHECKERR(gp_fputs(ESC LINEHEIGHT "16" FF ESC BIDIRECTIONAL ESC LINE8PI ESC ELITE,
                        gprn_stream),
                != 11,
                gs_error_ioerror)

        /* gp_fflush(...) returns void */
        gp_fflush(gprn_stream);

        lasterr = gs_error_ok;

        xit:
        gs_free(pdev->memory, (char*)prn, pdev->color_info.num_components*in_size, 1, "admp_print_page(prn)");
        gs_free(pdev->memory, (char*)bufT, pdev->color_info.num_components*in_size, 1, "admp_print_page(bufT)");
        gs_free(pdev->memory, (char*)bufK, pdev->color_info.num_components*in_size, 1, "admp_print_page(bufK)");
        if (pdev->color_info.num_components == 4)
        {
                gs_free(pdev->memory, (char*)bufY, pdev->color_info.num_components*in_size, 1, "admp_print_page(bufY)");
                gs_free(pdev->memory, (char*)bufM, pdev->color_info.num_components*in_size, 1, "admp_print_page(bufM)");
                gs_free(pdev->memory, (char*)bufC, pdev->color_info.num_components*in_size, 1, "admp_print_page(bufC)");
        }
        return_error(lasterr);
}
