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

/* April 2021 - January 2023 (so far):
 * Ghostscript Apple Dot Matrix Printer / ImageWriter driver v2.0
 *
 * This is a draft or alpha release of version ][ of the Apple dot matrix
 * printer Ghostscript driver series.
 *
 * Support and inspiration from the folks on the Ghostscript developers list,
 * including (in no particular order): Chris, Ray , Ken, Robin and William.
 *
 * This revision represents a major feature upgrade to the earlier drivers,
 * while retaining as much of the original code as practical.  Notable new
 * features include:
 * - Color support for the ImageWriter II and ImageWriter LQ
 *   (Not fully implemented.)
 * - User selectable rendering mode: gdev_prn_get_lines (experimental,
 *   -dRenderMode = 1) or gdev_prn_copy_scan_lines (standard,
 *   -dRenderMode = 0.)
 * - User selectable print-head directionality. (-dUnidirectional, bidirectional
 *   by default.)
 * - Support for multiple paper bins on the ImageWriter LQ.
 *   (Not yet implemented.)
 * - Enhanced job end and printer reset functionality for
 *   ImageWriter and newer (-dPlus.)
 * - A more robust driver with better error checking and also
 *   better optimized for Ghostscript.
 * - Improved comments and printer command readability.
 * - Other internal changes.
 *
 * This driver release is part of the larger "Guide to Running Apple Dot Matrix
 * Printers with Windows and UN*X Clients, A", which is published at
 * http://jmoyer.nodomain.net/admp/.  See also the "Apple Dot Matrix Printers"
 * Facebook group at https://www.facebook.com/groups/appledotmatrixprinters.
 *
 * Josh Moyer (he/him) <JMoyer@NODOMAIN.NET>
 * http://www.nodomain.net/
 */

 /* Oct 2019 - April 2021, maintained by Mike Galatean
  * (contact through https ://bugs.ghostscript.com )
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
 */

#include "gdevprn.h"
#include "std.h"
#include "string.h"

/* From gdevpbm.c */
#include "gdevprn.h"
#include "gscdefs.h"
#include "gscspace.h" /* For pnm_begin_typed_image(..) */
#include "gxgetbit.h"
#include "gxlum.h"
#include "gxiparam.h" /* For pnm_begin_typed_image(..) */
#include "gdevmpla.h"
#include "gdevplnx.h"
#include "gdevppla.h"

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
#define GPGL 1 /* gdev_prn_get_lines */
#define GPCSL 0 /* gdev_prn_copy_scan_lines */

/* Color coding modes */
#define PKSMRAWPPM 0

/* Color info modes */
#define FOURBITCMYK 2
#define ONEBITCMYK 1
#define MONO 0

/* Device command macros */
#define ESC "\033"
#define CR "\r"
#define LF "\n"
#define FF "\f"

#define RESET "c"

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
#define ERRDEVTYPE DRIVERNAME ": Driver determination failed.\n"
#define ERRDIRSELECT DRIVERNAME ": Printer initialization (directionality) failed.\n"
#define ERRREZSELECT DRIVERNAME ": Printer initialization (resolution) failed.\n"
#define ERRCOLORSELECT DRIVERNAME ": Printhead color positioning failed.\n"
#define ERRBLANKLINE DRIVERNAME ": Blank-line generation failed.\n"
#define ERRGPCSL DRIVERNAME ": Error getting scanline from rendering engine.\n"
#define ERRNUMCOMP DRIVERNAME ": color_info.num_components out of range.\n"
#define ERRGPGL DRIVERNAME ": gdev_prn_get_lines() returned unexpected values.\n"
#define ERRADVNLQ DRIVERNAME ": Near letter quality vertical advance failure."
#define ERRCMDLQ DRIVERNAME ": Letter quality command failure.\n"
#define ERRCMDNLQ DRIVERNAME ": Near letter quality command failure.\n"
#define ERRCMD DRIVERNAME ": Command failure.\n"
#define ERRDATLQ DRIVERNAME ": Letter quality data failure.\n"
#define ERRDATNLQ DRIVERNAME ": Near letter quality data failure.\n"
#define ERRDAT DRIVERNAME ": Data failure.\n"
#define ERRLHNLQ DRIVERNAME ": Near letter quality line height failure."
#define ERRPOSLQ DRIVERNAME ": Letter quality positionining failure.\n"
#define ERRPOSNLQ DRIVERNAME ": Near letter quality positionining failure.\n"
#define ERRPOS DRIVERNAME ": Positionining failure.\n"
#define ERRCR DRIVERNAME ": Carriage return failed.\n"
#define ERRCRLF DRIVERNAME ": Carriage return and line feed failed.\n"
#define ERRRESETPLUS DRIVERNAME ": Plus reset failure.\n"
#define ERRRESETHACK DRIVERNAME ": Reset hack failure.\n"
#define ERRRESET DRIVERNAME ": Reset failure.\n"

/* Device structure */
struct gx_device_admp_s {
        gx_device_common;
        gx_prn_device_common;
        bool color;
        bool plus;
        bool unidirectional;
        int rendermode;
        int colorinfo;
        int colorcoding;
        bool debug;
        bool UsePlanarBuffer;       /* 0 if chunky buffer, 1 if planar */
};

typedef struct gx_device_admp_s gx_device_admp;

/* Device procedure initialization */
static dev_proc_put_params(admp_put_params);
static dev_proc_get_params(admp_get_params);
static dev_proc_print_page(admp_print_page);
//static dev_proc_open_device(ppm_open);
//static dev_proc_get_params(ppm_get_params);
//static dev_proc_put_params(ppm_put_params);

/*
static int
ppm_open(gx_device* pdev)
{
        outprintf(pdev->memory, "ppm_open()\n");

        gx_device_admp* bdev = (gx_device_admp*)pdev;
        int code;

#ifdef TEST_PAD_AND_ALIGN
        pdev->pad = 5;
        pdev->log2_align_mod = 6;
#endif

        code = gdev_prn_open_planar(pdev, bdev->UsePlanarBuffer);
        while (pdev->child)
                pdev = pdev->child;

        bdev = (gx_device_admp*)pdev;

        if (code < 0)
                return code;
        pdev->color_info.separable_and_linear = GX_CINFO_SEP_LIN;
        set_linear_color_bits_mask_shift(pdev);
        //bdev->uses_color = 0;
        //bdev->color = 0;
        
        /* if (((gx_device_admp*)pdev)->color)
                ppm_set_dev_procs(pdev);*/
/*
        return code;
}

static int
ppm_get_params(gx_device* pdev, gs_param_list* plist)
{
        gx_device_admp* const bdev = (gx_device_admp*)pdev;
        int code;

        code = gdev_prn_get_params_planar(pdev, plist, &bdev->UsePlanarBuffer);
        if (code < 0) return code;
        //code = param_write_null(plist, "OutputIntent");
        //return code;
}

static int
ppm_put_params(gx_device* pdev, gs_param_list* plist)
{
        gx_device_admp* const bdev = (gx_device_admp*)pdev;
        gx_device_color_info save_info;
        int ncomps = pdev->color_info.num_components;
        int bpc = pdev->color_info.depth / ncomps;
        int ecode = 0;
        int code;
        long v;
        gs_param_string_array intent;
        const char* vname;
*/
        /*
        if ((code = param_read_string_array(plist, "OutputIntent", &intent)) == 0) {
                /* This device does not use the OutputIntent parameter.
                   We include this code just as a sample how to handle it.
                   The PDF interpreter extracts OutputIntent from a PDF file and sends it to here,
                   if a device includes it in the .getdeviceparams response.
                   This device does include due to ppm_get_params implementation.
                   ppm_put_params must handle it (and ingore it) against 'rangecheck'.
                 *
                static const bool debug_print_OutputIntent = false;

                if (debug_print_OutputIntent) {
                        int i, j;

                        dmlprintf1(pdev->memory, "%d strings:\n", intent.size);
                        for (i = 0; i < intent.size; i++) {
                                const gs_param_string* s = &intent.data[i];
                                dmlprintf2(pdev->memory, "  %d: size %d:", i, s->size);
                                if (i < 4) {
                                        for (j = 0; j < s->size; j++)
                                                dmlprintf1(pdev->memory, "%c", s->data[j]);
                                }
                                else {
                                        for (j = 0; j < s->size; j++)
                                                dmlprintf1(pdev->memory, " %02x", s->data[j]);
                                }
                                dmlprintf(pdev->memory, "\n");
                        }
                }
        }
        save_info = pdev->color_info;
        /*
        if ((code = param_read_long(plist, (vname = "GrayValues"), &v)) != 1 ||
                (code = param_read_long(plist, (vname = "RedValues"), &v)) != 1 ||
                (code = param_read_long(plist, (vname = "GreenValues"), &v)) != 1 ||
                (code = param_read_long(plist, (vname = "BlueValues"), &v)) != 1
                ) {
                if (code < 0)
                        ecode = code;
                else if (v < 2 || v >(bdev->is_raw || ncomps > 1 ? 256 : 65536L))
                        param_signal_error(plist, vname,
                                ecode = gs_error_rangecheck);
                else if (v == 2)
                        bpc = 1;
                else if (v <= 4)
                        bpc = 2;
                else if (v <= 16)
                        bpc = 4;
                else if (v <= 32 && ncomps == 3)
                        bpc = 5;
                else if (v <= 256)
                        bpc = 8;
                else
                        bpc = 16;
                if (ecode >= 0) {
                        static const byte depths[4][16] =
                        {
                            {1, 2, 0, 4, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 16},
                            {0},
                            {4, 8, 0, 16, 16, 0, 0, 24},
                            {4, 8, 0, 16, 0, 0, 0, 32},
                        };

                        pdev->color_info.depth = depths[ncomps - 1][bpc - 1];
                        pdev->color_info.max_gray = pdev->color_info.max_color =
                                (pdev->color_info.dither_grays =
                                        pdev->color_info.dither_colors = (int)v) - 1;
                }
        }
        */
        /*if (
                (code = ecode) < 0 ||
                (*/
                        //code = gdev_prn_put_params_planar(pdev, plist, &bdev->UsePlanarBuffer)
                 /*) < 0
            )
                pdev->color_info = save_info;
                *//*
        ppm_set_dev_procs(pdev);
        return code;
}
*/

static void
admp_initialize_device_procs(gx_device* pdev)
{
        gdev_prn_initialize_device_procs_mono(pdev);
        set_dev_proc(pdev, get_params, admp_get_params);
        set_dev_proc(pdev, put_params, admp_put_params);
}

static void
admp_initialize_device_procs_color(gx_device* pdev)
{
        gdev_prn_initialize_device_procs(pdev);

        //set_dev_proc(pdev, map_rgb_color, cmyk_1bit_map_cmyk_color);
        //set_dev_proc(pdev, map_color_rgb, NULL);
        //set_dev_proc(pdev, map_cmyk_color, cmyk_1bit_map_cmyk_color);
        set_dev_proc(pdev, decode_color, cmyk_1bit_map_color_rgb);
        set_dev_proc(pdev, encode_color, cmyk_1bit_map_cmyk_color);
        set_dev_proc(pdev, get_params, admp_get_params);
        set_dev_proc(pdev, put_params, admp_put_params);
        //set_dev_proc(pdev, open_device, ppm_open);
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
//                                     /* maxcomp, depth, maxgray, maxcolor, numgray, numcolor */
//      4, 4, 1, 1, 2, 2,              /* dci_macro(4,4) (also per pk[s]m[raw] device descriptor) */
//      4, 1, 1, 0, 2, 1,              /* dci_macro(4,1) */
        1, 1, 1, 0, 2, 1,              /* dci_macro(dci_black_and_white) */
        admp_print_page),
        /* colorbool,+ , directionbool, renderingmode,   colorinfo,   colorcoding, debug, UsePlanarBuffer */
                   0,1,             0,      GPCSL,  MONO, 0,     0,               0};

/* LQ hirez ImageWriter device descriptor */
gx_device_admp far_data gs_iwlqc_device = {
prn_device_margins_body(gx_device_admp, admp_initialize_device_procs_color, "iwlqc",	/* The print_page proc is compatible with allowing bg printing */
        DEFAULT_WIDTH_10THS_US_LETTER,  /* width_10ths, 8.5" */
        DEFAULT_HEIGHT_10THS_US_LETTER, /* height_10ths, 11" */
        320, 216,                       /* X_DPI, Y_DPI */
        0, 0, 0, 0, 0, 0,               /* origin and margins */
// gx_device_color_info - gxdevcli.h:258
//                                     /* maxcomp, depth, maxgray, maxcolor, numgray, numcolor */
//      4, 4, 1, 1, 2, 2,              /* dci_macro(4,4) (also per pk[s]m[raw] device descriptor) */
        4, 1, 1, 0, 2, 1,              /* dci_macro(4,1) */
//      1, 1, 1, 0, 2, 1,              /* dci_macro(dci_black_and_white) */
        admp_print_page),
        /* colorbool,+ , directionbool, renderingmode,   colorinfo,   colorcoding, debug, UsePlanarBuffer */
                   1,1,             0,      GPCSL,  ONEBITCMYK, PKSMRAWPPM,     0,               0 };

static int
admp_get_params(gx_device* pdev, gs_param_list* plist)
{
        outprintf(pdev->memory, "admp_get_params()\n");

        int code = gdev_prn_get_params(pdev, plist);

        if (code < 0 ||
                (code = param_write_bool(plist, "UsePlanarBuffer", &((gx_device_admp*)pdev)->UsePlanarBuffer)) < 0 ||
                (code = param_write_bool(plist, "Color", &((gx_device_admp*)pdev)->color)) < 0 ||
                (code = param_write_bool(plist, "Plus", &((gx_device_admp*)pdev)->plus)) < 0 ||
                (code = param_write_bool(plist, "Unidirectional", &((gx_device_admp*)pdev)->unidirectional)) < 0 ||
                (code = param_write_int(plist, "RenderMode", &((gx_device_admp*)pdev)->rendermode)) < 0 ||
                (code = param_write_int(plist, "ColorInfo", &((gx_device_admp*)pdev)->colorinfo)) < 0 ||
                (code = param_write_int(plist, "ColorCoding", &((gx_device_admp*)pdev)->colorcoding)) < 0 ||
                (code = param_write_bool(plist, "Debug", &((gx_device_admp*)pdev)->debug)) < 0
                )
                return code;

        return code;
}

static int
admp_put_params(gx_device* pdev, gs_param_list* plist)
{
        outprintf(pdev->memory, "admp_put_params()\n");

        /*
        pdev->color_info.separable_and_linear = GX_CINFO_SEP_LIN;
        set_linear_color_bits_mask_shift(pdev);
        */

        int code = 0;
        bool UsePlanarBuffer = ((gx_device_admp*)pdev)->UsePlanarBuffer;
        bool color = ((gx_device_admp*)pdev)->color;
        bool plus = ((gx_device_admp*)pdev)->plus;
        bool unidirectional = ((gx_device_admp*)pdev)->unidirectional;
        int rendermode = ((gx_device_admp*)pdev)->rendermode;
        int colorinfo = ((gx_device_admp*)pdev)->colorinfo;
        int colorcoding = ((gx_device_admp*)pdev)->colorcoding;
        bool debug = ((gx_device_admp*)pdev)->debug;
        gs_param_name param_name;
        int ecode = 0;
        /*
        gx_device_color_info color_info_4bit = dci_std_color_(4, 4);
        gx_device_color_info color_info_1bit = dci_std_color_(4, 1);
        gx_device_color_info color_info_mono = dci_black_and_white;
        */

        if ((code = param_read_bool(plist,
                (param_name = "Unidirectional"),
                &unidirectional)) < 0) {
                param_signal_error(plist, param_name, ecode = code);
        }
        if (ecode < 0)
                return code;

        if ((code = param_read_bool(plist,
                (param_name = "UsePlanarBuffer"),
                &UsePlanarBuffer)) < 0) {
                param_signal_error(plist, param_name, ecode = code);
        }
        if (ecode < 0)
                return code;

        if ((code = param_read_bool(plist,
                (param_name = "Color"),
                &color)) < 0) {
                param_signal_error(plist, param_name, ecode = code);
        }
        if (ecode < 0)
                return code;

        if ((code = param_read_bool(plist,
                (param_name = "Plus"),
                &plus)) < 0) {
                param_signal_error(plist, param_name, ecode = code);
        }
        if (ecode < 0)
                return code;

        if ((code = param_read_int(plist,
                (param_name = "RenderMode"),
                &rendermode)) < 0) {
                param_signal_error(plist, param_name, ecode = code);
        }
        if (ecode < 0)
                return code;

        if ((code = param_read_int(plist,
                (param_name = "ColorInfo"),
                &colorinfo)) < 0) {
                param_signal_error(plist, param_name, ecode = code);
        }
        if (ecode < 0)
                return code;

        if ((code = param_read_int(plist,
                (param_name = "ColorCoding"),
                &colorcoding)) < 0) {
                param_signal_error(plist, param_name, ecode = code);
        }
        if (ecode < 0)
                return code;

        if ((code = param_read_bool(plist,
                (param_name = "Debug"),
                &debug)) < 0) {
                param_signal_error(plist, param_name, ecode = code);
        }
        if (ecode < 0)
                return code;
        
        ((gx_device_admp*)pdev)->UsePlanarBuffer = UsePlanarBuffer;
        ((gx_device_admp*)pdev)->color = color;
        ((gx_device_admp*)pdev)->plus = plus;
        ((gx_device_admp*)pdev)->unidirectional = unidirectional;
        ((gx_device_admp*)pdev)->rendermode = rendermode;
        ((gx_device_admp*)pdev)->colorinfo = colorinfo;
        ((gx_device_admp*)pdev)->colorcoding = colorcoding;
        ((gx_device_admp*)pdev)->debug = debug;
        
        /* try to reverse engineer and dynamically set color_info values */
        /*if (((gx_device_admp*)pdev)->color)
        {
                switch (((gx_device_admp*)pdev)->colorinfo)
                {
                case FOURBITCMYK:
                        pdev->color_info = color_info_4bit;
                        break;
                case ONEBITCMYK:
                        pdev->color_info = color_info_1bit;
                        break;
                default: pdev->color_info = color_info_mono; // MONO
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
        else*/
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
        int code = gs_error_ok;
        char pcmd[255];
        int dev_rez;
        int dev_type;
        unsigned char color = 0;
        int line_size_mono = gdev_mem_bytes_per_scan_line((gx_device*)pdev); /* or bitmap_raster(pdev->width); */
        int in_size_mono = line_size_mono * 8; /* Note that in_size is a multiple of 8 dots in height. */

        char color_order[4];

        gx_color_index yellow_mask, cyan_mask, magenta_mask, black_mask;
        gx_render_plane_t yellow_plane, cyan_plane, magenta_plane, black_plane;
        gx_render_plane_t render_plane;
        byte* buf_y, * buf_c, * buf_m, * buf_k, * buf_in, * buf_out, * prn;
        int render_plane_num = 0;

        switch (pdev->color_info.num_components)
        {
        case 4:
                color_order[0] = 1;
                color_order[1] = 3;
                color_order[2] = 2;
                color_order[3] = 0;

                /*
                if (((gx_device_admp*)pdev)->rendermode == GPGL)
                {
                        gx_render_plane_init(&yellow_plane, (gx_device*)pdev, 2);
                        gx_render_plane_init(&cyan_plane, (gx_device*)pdev, 0);
                        gx_render_plane_init(&magenta_plane, (gx_device*)pdev, 1);
                }
                */

                buf_y = (byte*)gs_malloc(pdev->memory, in_size_mono * 3, 1, "admp_print_page(buf_y)");
                buf_c = (byte*)gs_malloc(pdev->memory, in_size_mono * 3, 1, "admp_print_page(buf_c)");
                buf_m = (byte*)gs_malloc(pdev->memory, in_size_mono * 3, 1, "admp_print_page(buf_m)");
                //render_plane_num = 3;
                break;
        case 1:
                color_order[0] = 0;
                color_order[1] = 0;
                color_order[2] = 0;
                color_order[3] = 0;

                /*
                if (((gx_device_admp*)pdev)->rendermode == GPGL)
                {
                        gx_render_plane_init(&render_plane, (gx_device*)pdev, render_plane_num);
                }
                else
                {
                        int render_plane_num = 0;
                }
                */

                break;
        default:
                outprintf(pdev->memory, ERRNUMCOMP);
                code = gs_note_error(gs_error_rangecheck);
                goto xit;
        }

        buf_k = (byte*)gs_malloc(pdev->memory,  in_size_mono * 3, 1, "admp_print_page(buf_k)");
        buf_in = (byte*)gs_malloc(pdev->memory, in_size_mono * pdev->color_info.num_components, 1, "admp_print_page(buf_in)");
        buf_out = (byte*)gs_malloc(pdev->memory, in_size_mono * pdev->color_info.num_components, 1, "admp_print_page(buf_out)");
        prn  = (byte*)gs_malloc(pdev->memory,  in_size_mono * 3, 1, "admp_print_page(prn)");

        byte* in = buf_in;
        byte* out = buf_out;
        int lnum = 0;
        int band_end;

        /* Check allocations */
        if (buf_k == 0 ||
            buf_in == 0 ||
            buf_out == 0 ||
            prn == 0 ||
            (gx_device_has_color(pdev) &&
                    (buf_y == 0 ||
                            buf_m == 0 ||
                            buf_c == 0)))
        {
                errprintf(pdev->memory, ERRALLOC);
                code = gs_note_error(gs_error_VMerror);
                goto xit;
        }

        /* Determine device type */
        /* will need modification if new device descriptors added */
        if (strcmp(pdev->dname, "iwlqc") == 0) dev_type = IWLQ;
        else if (strcmp(pdev->dname, "iwlq") == 0) dev_type = IWLQ;
        else if (strcmp(pdev->dname, "iwhi") == 0) dev_type = IWHI;
        else if (strcmp(pdev->dname, "iwlo") == 0) dev_type = IWLO;
        else if (strcmp(pdev->dname, "appledmp") == 0) dev_type = DMP;
        else
        {
                errprintf(pdev->memory, ERRDEVTYPE);
                code = gs_note_error(gs_error_rangecheck);
                goto xit;
        }

        /* Initialize the printer. */
        if (((gx_device_admp*)pdev)->unidirectional)
        {
                (void)strcpy(pcmd, ESC UNIDIRECTIONAL ESC LINEHEIGHT "16");
                code = gp_fputs(pcmd, gprn_stream);
        }
        else
        {
                (void)strcpy(pcmd, ESC BIDIRECTIONAL ESC LINEHEIGHT "16");
                code = gp_fputs(pcmd, gprn_stream);
        }

        if (code != strlen(pcmd))
        {
                errprintf(pdev->memory, ERRDIRSELECT);
                code = gs_note_error(gs_error_ioerror);
                goto xit;
        }

        /* Set resolution */
        if (pdev->y_pixels_per_inch == 216) dev_rez = H320V216;
        else if (pdev->y_pixels_per_inch == 144) dev_rez = H160V144;
        else if (pdev->x_pixels_per_inch == 160) dev_rez = H160V72;
        else dev_rez = H120V72;

        /* Set character pitch */
        switch (dev_rez)
        {
        case H320V216:
                (void)strcpy(pcmd, ESC ELITEPROPORTIONAL ESC LQPROPORTIONAL);
                code = gp_fputs(pcmd, gprn_stream);
                break;
        case H160V144:
        case H160V72:
                (void)strcpy(pcmd, ESC ELITEPROPORTIONAL);
                code = gp_fputs(pcmd, gprn_stream);
                break;
        case H120V72:
        default:
                (void)strcpy(pcmd, ESC CONDENSED);
                code = gp_fputs(pcmd, gprn_stream);
                break;
        }

        if (code != strlen(pcmd))
        {
                errprintf(pdev->memory, ERRREZSELECT);
                code = gs_note_error(gs_error_ioerror);
                goto xit;
        }

        /* Print lines of graphics */
        while (lnum < pdev->height)
        {
                byte* inp;
                byte* in_end;
                byte* out_end;
                int lcnt, ltmp;
                int count, passes;
                byte* prn_blk, * prn_end, * prn_tmp, * prn_orig;

                /* 8-dot tall bands per pass */
                switch (dev_rez)
                {
                case H320V216: passes = 3; break;
                case H160V144: passes = 2; break;
                case H160V72:
                case H120V72:
                default: passes = 1; break;
                }
                //gx_render_plane_init(&render_plane, (gx_device*)pdev, 0);

                /* get rendered scanlines and assemble into 8, 16, or 24-dot high strips */
                for (count = 0; count < passes; count++)
                {
                        for (lcnt = 0; lcnt < 8; lcnt++)
                        {
                                byte* row;
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
                                        code = (int)memset(in + lcnt * line_size_mono * pdev->color_info.num_components, 0, line_size_mono * pdev->color_info.num_components);

                                        if (code != (int)(in + lcnt * line_size_mono * pdev->color_info.num_components))
                                        {
                                                errprintf(pdev->memory, ERRBLANKLINE);
                                                code = gs_note_error(gs_error_undefined);
                                                goto xit;
                                        }
                                }
                                else /* otherwise, get scan lines */
                                {
                                        switch (((gx_device_admp*)pdev)->rendermode)
                                        {
                                        case GPGL:
                                                /*gx_color_usage_t color_usage;
                                                int band_start;
                                                int band_height =
                                                        gdev_prn_color_usage((gx_device*)pdev, lnum, 1,
                                                                &color_usage, &band_start);

                                                band_end = band_start + band_height;
                                                marked = color_usage. or &(plane_mask << plane_shift);
                                                if (!marked)
                                                        memset(in, 0, line_size);
                                                */

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

                                                gx_render_plane_t render_plane;
                                                uint actual_line_size;
                                                byte* actual_buffer;

                                                render_plane.index = color;
                                                byte* row;

                                                //if (((gx_device_admp*)pdev)->debug) outprintf(pdev->memory, "pre-out: %d", out);
                                                code = (int)gdev_prn_get_lines(pdev,
                                                        lnum + ltmp,
                                                        1,
                                                        //out, //produces some initial output, then lots of 0xCD ~700KB
                                                        out + line_size_mono * pdev->color_info.num_components * (7 - lcnt), // closest to original call, produces some partially sane output, but has lots of extra whitespace on long lines ~7KB w/o 0xCD
                                                        //out + line_size_mono * pdev->color_info.num_components * lcnt, // produces some partially sane output, but has lots of extra whitespace on long lines ~7KB w/o 0xCD
                                                        //in + line_size * (7 - lcnt),
                                                        line_size_mono,// *pdev->color_info.num_components,
                                                        &actual_buffer,
                                                        &actual_line_size,
                                                        &render_plane); // providing NULL resulted in all 0xCD output ~800KB
                                                //if (((gx_device_admp*)pdev)->debug) outprintf(pdev->memory, ", post-out: %d\n", out);

                                                if (((gx_device_admp*)pdev)->debug) outprintf(pdev->memory, "code=%d,actual_line_size=%d\n", code, actual_line_size);

                                                if (((gx_device_admp*)pdev)->debug && actual_buffer != out + line_size_mono * pdev->color_info.num_components * lcnt)
                                                        outprintf(pdev->memory, "out=%x,out+line_size_mono * pdev->color_info.num_components+lcnt=%x,actual_buffer=%x\n", out, out + line_size_mono * pdev->color_info.num_components * lcnt, actual_buffer);

                                                if (code = gs_error_rangecheck) continue;

                                                if (code != 1 && actual_line_size != line_size_mono * pdev->color_info.num_components)
                                                {
                                                        errprintf(pdev->memory, ERRGPGL);
                                                        code = gs_note_error(gs_error_rangecheck);
                                                        goto xit;
                                                }
                                                break;
                                        case GPCSL:
                                                /* gdev_prn_copy_scan_lines(gx_device_printer * pdev, int y, byte * str, uint size) - gdevprn.c:1641 */
                                                /* copy scanlines to the buffer */
                                                code = gdev_prn_copy_scan_lines(
                                                        pdev,
                                                        (lnum + ltmp),
                                                        /*((lnum*(color+1)) + ltmp),*/
                                                        in + (line_size_mono * pdev->color_info.num_components) * (7 - lcnt), /*( lcnt * (color + 1))),*/
                                                        line_size_mono * pdev->color_info.num_components);// *pdev->color_info.num_components);

                                                if (code != pdev->color_info.num_components)
                                                {
                                                        errprintf(pdev->memory, "code=%d,line_size_mono=%d\n", code, line_size_mono);
                                                        errprintf(pdev->memory, ERRGPCSL);
                                                        code = gs_note_error(gs_error_rangecheck);
                                                        goto xit;
                                                }

                                                break;
                                        }
                                        if (lnum == 24)
                                        {
                                                int foo = 0;
                                        }
                                        if (((gx_device_admp*)pdev)->debug && lnum > 64) goto xit;
                                }
                        } /* for lcnt */

                        if (((gx_device_admp*)pdev)->debug) gp_fflush(gprn_stream); /* flush the file here to guarantee output earlier in the pipeline while debugging */

                        //gx_color_index marked = 0;
                        //int plane_depth;
                        //int plane_shift;
                        //gx_color_index plane_mask;

                        //plane_depth = render_plane.depth;
                        //plane_shift = render_plane.shift;
                        //plane_mask = ((gx_color_index)1 << plane_depth) - 1;
                        //raster = bitmap_raster(pdev->width * plane_depth);

                        /* Here we have a full input buffer, which supposedly requires transposition -- requiring another copy operation */

                        switch (((gx_device_admp*)pdev)->rendermode)
                        {
                        case GPCSL:
                                out_end = out;
                                inp = in;// +(line_size * color);
                                in_end = inp + (line_size_mono * pdev->color_info.num_components);// +(line_size * color);
                                for (; inp < in_end; inp++, out_end += 8)
                                {
                                        /* The apple DMP printer seems to be odd in that the bit order on
                                         * each line is reverse what might be expected.  Meaning, an
                                         * underscore would be done as a series of 0x80, while on overscore
                                         * would be done as a series of 0x01.  So we get each
                                         * scan line in reverse order. -- Unknown
                                         */
                                         /* gdev_prn_transpose(...) returns void */
                                        gdev_prn_transpose_8x8(inp, line_size_mono * pdev->color_info.num_components, out_end, 1);

                                }
                                break;
                        }

                        /*
                        So, here we have the output buffer, fully transposed, supposedly...
                        if we take this output buffer and copy it to the color buffers
                        */
                        out_end = out;

                        byte* out_saved = out;
                        for (color = 0; color < pdev->color_info.num_components; color++)
                        {

                                out = out_saved;

                                switch (color)
                                {
                                case 0:
                                        prn = buf_k;
                                        out = out;
                                        break;
                                case 1:
                                        prn = buf_y;
                                        out = out + line_size_mono;
                                        break;
                                case 2:
                                        prn = buf_m;
                                        out = out + (line_size_mono * 2);
                                        break;
                                case 3:
                                        prn = buf_c;
                                        out = out + (line_size_mono * 3);
                                        break;
                                default:
                                        outprintf(pdev->memory, ERRNUMCOMP);
                                        code = gs_note_error(gs_error_rangecheck);
                                        goto xit;
                                }

                                out_end = out;

                                switch (dev_rez)
                                {
                                case H320V216: prn_end = prn + count; break;
                                case H160V144: prn_end = prn + in_size_mono * count; break;
                                case H160V72:
                                case H120V72:
                                default: prn_end = prn; break;
                                }

                                //if (((gx_device_admp*)pdev)->debug) outprintf(pdev->memory, "color: %d; out_end: 0x%8x; out: 0x%8x;\n", color, out_end, out);
                                while ((int)(out_end - out) < in_size_mono)
                                {
                                        *prn_end = *(out_end++);
                                        if (dev_rez == H320V216) prn_end += 3;
                                        else prn_end++;
                                }
                                //if (((gx_device_admp*)pdev)->debug) outprintf(pdev->memory, "color: %d; out_end: 0x%8x; out: 0x%8x;\n", color, out_end, out);
                        } /* for color, first */

                        out = out_saved;
                } /* for count */

                for (color = 0 ; color < pdev->color_info.num_components; color++)
                {
                        /* Select ribbon color */
                        if (gx_device_has_color(pdev))
                        {
                                switch (color_order[color])
                                {
                                case 3: (void)strcpy(pcmd, ESC COLORSELECT CYAN); break;
                                case 2: (void)strcpy(pcmd, ESC COLORSELECT MAGENTA); break;
                                case 1: (void)strcpy(pcmd, ESC COLORSELECT YELLOW); break;
                                case 0: (void)strcpy(pcmd, ESC COLORSELECT BLACK); break;
                                }

                                code = gp_fputs(pcmd, gprn_stream);

                                if (code != strlen(pcmd))
                                {
                                        errprintf(pdev->memory, ERRCOLORSELECT);
                                        code = gs_note_error(gs_error_ioerror);
                                        goto xit;
                                }
                        }

                        switch (color)
                        {
                        case 0:
                                prn = buf_k;
                                break;
                        case 1:
                                prn = buf_y;
                                break;
                        case 2:
                                prn = buf_c;
                                break;
                        case 3:
                                prn = buf_m;
                                break;
                        default:
                                outprintf(pdev->memory, ERRNUMCOMP);
                                code = gs_note_error(gs_error_rangecheck);
                                goto xit;
                        }

                        //prn_orig = prn;

                        /* send bitmaps to printer, based on resolution setting

                                note: each of the three vertical resolutions has a
                                unique and separate implementation
                        */
                        switch (dev_rez)
                        {
                        case H320V216:
                                prn_blk = prn;
                                prn_end = prn_blk + in_size_mono * 3;
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
                                                /*code = */ gp_fprintf(gprn_stream,
                                                        ESC BITMAPHIRPT "%04d%c%c%c",
                                                        (int)((prn_blk - prn) / 3),
                                                        0, 0, 0);

                                                /*if (code != 9)
                                                {
                                                        errprintf(pdev->memory, ERRPOSLQ);
                                                        code = gs_note_error(gs_error_ioerror);
                                                        goto xit;
                                                }*/
                                        }
                                        else
                                                prn_blk = prn;

                                        /* send hirez bitmapped graphics mode command with length parameter*/
                                        code = gp_fprintf(gprn_stream,
                                                ESC BITMAPHI "%04d",
                                                (int)((prn_end - prn_blk) / 3));

                                        if (code < 6 || code > 7)
                                        {
                                                errprintf(pdev->memory, ERRCMDLQ);
                                                code = gs_note_error(gs_error_ioerror);
                                                goto xit;
                                        }

                                        /* send actual bitmap data */
                                        code = gp_fwrite(prn_blk,
                                                1,
                                                (int)(prn_end - prn_blk),
                                                gprn_stream);

                                        if (code != (int)(prn_end - prn_blk))
                                        {
                                                errprintf(pdev->memory, ERRDATLQ);
                                                code = gs_note_error(gs_error_ioerror);
                                                goto xit;
                                        }
                                }
                                break;
                        case H160V144:
                                for (count = 0; count < 2; count++)
                                {
                                        prn_blk = prn_tmp = prn + in_size_mono * count;
                                        prn_end = prn_blk + in_size_mono;
                                        while (prn_end > prn_blk && prn_end[-1] == 0)
                                                prn_end--;
                                        while (prn_blk < prn_end && prn_blk[0] == 0)
                                                prn_blk++;
                                        if (prn_end != prn_blk)
                                        {
                                                if ((prn_blk - prn_tmp) > 7)
                                                {
                                                        code = gp_fprintf(gprn_stream,
                                                                ESC BITMAPLORPT "%04d%c",
                                                                (int)(prn_blk - prn_tmp),
                                                                0);

                                                        if (code != 7)
                                                        {
                                                                errprintf(pdev->memory, ERRPOSNLQ);
                                                                code = gs_note_error(gs_error_ioerror);
                                                                goto xit;
                                                        }
                                                }
                                                else
                                                        prn_blk = prn_tmp;

                                                code = gp_fprintf(gprn_stream,
                                                        ESC BITMAPLO "%04d",
                                                        (int)(prn_end - prn_blk));

                                                if (code != 6)
                                                {
                                                        errprintf(pdev->memory, ERRCMDNLQ);
                                                        code = gs_note_error(gs_error_ioerror);
                                                        goto xit;
                                                }

                                                code = gp_fwrite(prn_blk,
                                                        1,
                                                        (int)(prn_end - prn_blk),
                                                        gprn_stream);

                                                if (code != (int)(prn_end - prn_blk))
                                                {
                                                        errprintf(pdev->memory, ERRDATNLQ);
                                                        code = gs_note_error(gs_error_ioerror);
                                                        goto xit;
                                                }
                                        }
                                        if (!count)
                                        {
                                                code = gp_fputs(ESC LINEHEIGHT "01" CR LF,
                                                        gprn_stream);

                                                if (code != 6)
                                                {
                                                        errprintf(pdev->memory, ERRADVNLQ);
                                                        code = gs_note_error(gs_error_ioerror);
                                                        goto xit;
                                                }
                                        }
                                }
                                code = gp_fputs(ESC LINEHEIGHT "15",
                                        gprn_stream);

                                if (code != 4)
                                {
                                        errprintf(pdev->memory, ERRLHNLQ);
                                        code = gs_note_error(gs_error_ioerror);
                                        goto xit;
                                }
                                break;
                        case H160V72:
                        case H120V72:
                        default:
                                prn_blk = prn;
                                prn_end = prn_blk + in_size_mono;
                                while (prn_end > prn_blk && prn_end[-1] == 0)
                                        prn_end--;
                                while (prn_blk < prn_end && prn_blk[0] == 0)
                                        prn_blk++;
                                if (prn_end != prn_blk)
                                {
                                        if ((prn_blk - prn) > 7)
                                        {
                                                code = gp_fprintf(gprn_stream,
                                                        ESC BITMAPLORPT "%04d%c",
                                                        (int)(prn_blk - prn),
                                                        0);

                                                if (code != 7)
                                                {
                                                        errprintf(pdev->memory, ERRPOS);
                                                        code = gs_note_error(gs_error_ioerror);
                                                        goto xit;
                                                }
                                        }
                                        else
                                                prn_blk = prn;

                                        code = gp_fprintf(gprn_stream,
                                                ESC BITMAPLO "%04d",
                                                (int)(prn_end - prn_blk));

                                        if (code != 6)
                                        {
                                                errprintf(pdev->memory, ERRCMD);
                                                code = gs_note_error(gs_error_ioerror);
                                                goto xit;
                                        }

                                        code = gp_fwrite(prn_blk,
                                                1,
                                                (int)(prn_end - prn_blk),
                                                gprn_stream);

                                        if (code != (int)(prn_end - prn_blk))
                                        {
                                                errprintf(pdev->memory, ERRDAT);
                                                code = gs_note_error(gs_error_ioerror);
                                                goto xit;
                                        }
                                }
                                break;
                        }


                        if (color < 3)
                        {
                                /* temporarily inject an LF with the CR for debugging */
                                code = gp_fputs(CR LF, gprn_stream);
                                if (code != 2)
                                {
                                        errprintf(pdev->memory, ERRCR);
                                        code = gs_note_error(gs_error_ioerror);
                                        goto xit;
                                }
                        }
                        else
                        {
                                code = gp_fputs(CR LF, gprn_stream);
                                if (code != 2)
                                {
                                        errprintf(pdev->memory, ERRCRLF);
                                        code = gs_note_error(gs_error_ioerror);
                                        goto xit;
                                }
                        }

                } /* for color, second */

                switch (dev_rez)
                {
                case H320V216: lnum += 24; break;
                case H160V144: lnum += 16; break;
                case H160V72:
                case H120V72:
                default: lnum += 8; break;
                }
        } /* while lnum < pdev->height */

        /* Eject the page and reset the printer.
         *
         * For ImageWriter and newer printers, use Plus mode paper handling
         * and printer reset commands, available in driver version ][.
         * Otherwise use the original code.
         */
        if (((gx_device_admp*)pdev)->plus)
        {
                code = gp_fputs(FF ESC RESET,
                        gprn_stream);

                if (code != 3)
                {
                        errprintf(pdev->memory, ERRRESETPLUS);
                        code = gs_note_error(gs_error_ioerror);
                        goto xit;
                }
        }
        else
        {
                /* ImageWriter will skip a whole page if too close to end */
                /* so skip back more than an inch */
                if ( !(dev_type == DMP) )
                {
                        code = gp_fputs(ESC LINEHEIGHT "99" LF LF ESC LINEFEEDREV LF LF LF LF ESC LINEFEEDFWD,
                                        gprn_stream);
                        if (code != 14)
                        {
                                errprintf(pdev->memory, ERRRESETHACK);
                                code = gs_note_error(gs_error_ioerror);
                                goto xit;
                        }
                }

                /* Formfeed and Reset printer */
                code = gp_fputs(ESC LINEHEIGHT "16" FF ESC BIDIRECTIONAL ESC LINE8PI ESC ELITE,
                        gprn_stream);

                if (code != 11)
                {
                        errprintf(pdev->memory, ERRRESET);
                        code = gs_note_error(gs_error_ioerror);
                        goto xit;
                }
        }

        /* gp_fflush(...) returns void */
        gp_fflush(gprn_stream);

        code = gs_error_ok;

xit:
        gs_free(pdev->memory, (char*)prn, in_size_mono * 3, 1, "admp_print_page(prn)");
        gs_free(pdev->memory, (char*)buf_out, in_size_mono * pdev->color_info.num_components, 1, "admp_print_page(buf_out)");
        gs_free(pdev->memory, (char*)buf_in, in_size_mono * pdev->color_info.num_components, 1, "admp_print_page(buf_in)");
        gs_free(pdev->memory, (char*)buf_k, in_size_mono * 3, 1, "admp_print_page(buf_k)");
        if (gx_device_has_color(pdev))
        {
                gs_free(pdev->memory, (char*)buf_m, in_size_mono * 3, 1, "admp_print_page(buf_m)");
                gs_free(pdev->memory, (char*)buf_c, in_size_mono * 3, 1, "admp_print_page(buf_c)");
                gs_free(pdev->memory, (char*)buf_y, in_size_mono * 3, 1, "admp_print_page(buf_y)");
        }
        return_error(code);
        }