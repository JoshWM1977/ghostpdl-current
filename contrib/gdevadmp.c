/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/

/* April 2021 - September 2023 in The Emerald City:
 * ghostscript Apple Dot Matrix Printer / ImageWriter driver, version ][
 *
 * This is the pre-beta or pre-release of version ][ of the Apple dot-matrix
 * printers driver for ghostscript -- it is not fully tested yet and could
 * jam or even damage your printer.
 *
 * Thanks for support and inspiration to the folks on the ghostscript
 * developers list, including (in no particular order):
 * Chris, Ray, Ken, Robin and William!
 *
 * This release represents a major feature upgrade and overhaul, as compared 
 * to the earlier drivers, while retaining as much of the original code as
 * practical.  Notable changes and new features include:
 * - Color support for the ImageWriter II and ImageWriter LQ!
 * - New rendering call: gdev_prn_get_lines() based, instead of
 *   gdev_prn_copy_scan_lines().
 * - User selectable print-head directionality. (-dUNIDIRECTIONAL -- 
 *   bidirectional by default.)
 * - Support for multiple paper bins on the ImageWriter LQ.
 *   (Not yet implemented.)
 * - Improved paper handling: rather than issuing a form-feed at the
 *   end of a page, just line-feed through the entire length, instead.
 * - Safer default margins and printer settings that match the documented
 *   and recommended values.
 * - A more robust driver with better error checking and also
 *   better optimized for ghostscript.
 * - Improved comments and printer command readability.
 * - Other minor and miscellaneos changes.
 * 
 * Known issues include:
 * - Users can select unsupported color and resolution settings for their device.
 *   Or to put another way, a DMP user could select color or H320V216 and get
 *   unpredictable or even dangerous output.
 *
 * This driver release is part of the larger "Guide to Running Apple Dot Matrix
 * Printers with Windows and UN*X Clients, A", which is published at
 * http://jmoyer.nodomain.net/admp/.  See also the "Apple Dot Matrix Printers"
 * Facebook group at https://www.facebook.com/groups/appledotmatrixprinters.
 *
 *    _____    Josh Moyer (he/him) <JMoyer@NODOMAIN.NET>
 *   | * * |   http://jmoyer.nodomain.net/
 *   |*(*)*|   http://www.nodomain.net/
 *    \ - /
 *     \//     Love, Responsibility, Justice
 *             Liebe, Verantwortung, Gerechtigkeit
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

 /* Device type macros */
#define DMP 0
#define IWLO 1
#define IWHI 2
#define IWLQ 4

/* Device resolution macros */
#define H120V72 8
#define H160V72 16
#define H160V144 32
#define H320V216 64

/* Device rendering modes */
#define GPGL 1 /* gdev_prn_get_lines */
#define GPCSL 0 /* gdev_prn_copy_scan_lines */

/* Device command macros */
#define ESC "\033"
#define CR "\r"
#define LF "\n"

#define ELITE "E"
#define ELITEPROPORTIONAL "P"
#define CONDENSED "q"
#define LQPROPORTIONAL "a3"

#define BIDIRECTIONAL "<"
#define UNIDIRECTIONAL ">"

#define LINEHEIGHT "T"
#define LINE6PI "A"

#define BITMAPLO "G"
#define BITMAPLORPT "V"
#define BITMAPHI "C"
#define BITMAPHIRPT "U"

#define COLORSELECT "K"
#define YELLOW "1"      /* (in a recommended order of printing for */
#define CYAN "3"       /* minimizing cross-band color contamination) */
#define MAGENTA "2"
#define BLACK "0"

#define DRIVERNAME "gdevadmp"
#define ERRADVNLQ DRIVERNAME ": Near letter quality vertical advance failure."
#define ERRALLOC DRIVERNAME ": Memory allocation failed.\n"
#define ERRCMD DRIVERNAME ": Command failure.\n"
#define ERRCMDLQ DRIVERNAME ": Letter quality command failure.\n"
#define ERRCMDNLQ DRIVERNAME ": Near letter quality command failure.\n"
#define ERRCOLORSELECT DRIVERNAME ": Printhead color selection failed.\n"
#define ERRCR DRIVERNAME ": Carriage return failed.\n"
#define ERRCRLF DRIVERNAME ": Carriage return and line feed failed.\n"
#define ERRDAT DRIVERNAME ": Data failure.\n"
#define ERRDATLQ DRIVERNAME ": Letter quality data failure.\n"
#define ERRDATNLQ DRIVERNAME ": Near letter quality data failure.\n"
#define ERRDEVTYPE DRIVERNAME ": Driver determination failed.\n"
#define ERRDIRSELECT DRIVERNAME ": Printer initialization (directionality) failed.\n"
#define ERRGPCSL DRIVERNAME ": gdev_prn_copy_scan_lines returned an unexpected value: code=%d,line_size=%d\n"
#define ERRGPGL DRIVERNAME ": gdev_prn_get_lines() returned an unexpected value: %d\n"
#define ERRLHNLQ DRIVERNAME ": Near letter quality line height failure."
#define ERRMEMCOPY DRIVERNAME ": Error copying row to input buffer.\n"
#define ERRNUMCOMP DRIVERNAME ": color_info.num_components out of range.\n"
#define ERRPLANEINIT DRIVERNAME ": gx_render_plane_init returned an unexpected value: %d\n"
#define ERRPOS DRIVERNAME ": Positionining failure.\n"
#define ERRPOSLQ DRIVERNAME ": Letter quality positionining failure.\n"
#define ERRPOSNLQ DRIVERNAME ": Near letter quality positionining failure.\n"
#define ERRRESET DRIVERNAME ": Reset failure.\n"
#define ERRREZDEVMISMATCH DRIVERNAME ": the selected device does not support the selected resolution.\n"
#define ERRREZSELECT DRIVERNAME ": Printer initialization (resolution) failed.\n"
#define ERRZEROROW DRIVERNAME ": Error zeroing row buffer.\n"

/* Device structure */
struct gx_device_admp_s {
        gx_device_common;
        gx_prn_device_common;
        bool unidirectional;
        int rendermode;
};

typedef struct gx_device_admp_s gx_device_admp;

/* Device procedure initialization */
static dev_proc_put_params(admp_put_params);
static dev_proc_get_params(admp_get_params);
static dev_proc_print_page(admp_print_page);

static void
admp_initialize_device_procs(gx_device* pdev)
{
        gdev_prn_initialize_device_procs_mono_bg(pdev);

        set_dev_proc(pdev, get_params, admp_get_params);
        set_dev_proc(pdev, put_params, admp_put_params);
}

static void
admp_initialize_device_procs_color(gx_device* pdev)
{
        gdev_prn_initialize_device_procs_cmyk1_bg(pdev);

        set_dev_proc(pdev, get_params, admp_get_params);
        set_dev_proc(pdev, put_params, admp_put_params);
}

/* Standard DMP device descriptor */
const gx_device_admp far_data gs_appledmp_device = {
prn_device_margins_body(gx_device_admp, admp_initialize_device_procs, "appledmp",
        DEFAULT_WIDTH_10THS_US_LETTER,  /* width_10ths, 8.5" */
        DEFAULT_HEIGHT_10THS_US_LETTER, /* height_10ths, 11" */
        120, 72,                        /* X_DPI, Y_DPI */
        0.25, 0, 0.25, 0.25, 0.25, 0.25,/* origin and margins */
        1, 1, 1, 0, 2, 1,               /* maxcomp, depth, maxgray, maxcolor, numgray, numcolor */
        admp_print_page),
        0, 0 };                         /* unidirectional, render mode */


/*  lowrez ImageWriter device descriptor */
const gx_device_admp far_data gs_iwlo_device = {
prn_device_margins_body(gx_device_admp, admp_initialize_device_procs, "iwlo",
        DEFAULT_WIDTH_10THS_US_LETTER,  /* width_10ths, 8.5" */
        DEFAULT_HEIGHT_10THS_US_LETTER, /* height_10ths, 11" */
        160, 72,                        /* X_DPI, Y_DPI */
        0.25, 0, 0.25, 0.25, 0.25, 0.25,/* origin and margins */
        1, 1, 1, 0, 2, 1,               /* maxcomp, depth, maxgray, maxcolor, numgray, numcolor */
        admp_print_page),
        0, 0 };                         /* unidirectional, render mode */

/*  hirez ImageWriter device descriptor */
const gx_device_admp far_data gs_iwhi_device = {
prn_device_margins_body(gx_device_admp, admp_initialize_device_procs, "iwhi",
        DEFAULT_WIDTH_10THS_US_LETTER,  /* width_10ths, 8.5" */
        DEFAULT_HEIGHT_10THS_US_LETTER, /* height_10ths, 11" */
        160, 144,                       /* X_DPI, Y_DPI */
        0.25, 0, 0.25, 0.25, 0.25, 0.25,/* origin and margins */
        1, 1, 1, 0, 2, 1,               /* maxcomp, depth, maxgray, maxcolor, numgray, numcolor */
        admp_print_page),
        0, 0 };                         /* unidirectional, render mode */

/* color  hirez ImageWriter device descriptor */
const gx_device_admp far_data gs_iwhic_device = {
prn_device_margins_body(gx_device_admp, admp_initialize_device_procs_color, "iwhic",
        DEFAULT_WIDTH_10THS_US_LETTER,  /* width_10ths, 8.5" */
        DEFAULT_HEIGHT_10THS_US_LETTER, /* height_10ths, 11" */
        160, 144,                       /* X_DPI, Y_DPI */
        0.25, 0, 0.25, 0.25, 0.25, 0.25,/* origin and margins */
        4, 4, 1, 1, 2, 2,               /* maxcomp, depth, maxgray, maxcolor, numgray, numcolor */
        admp_print_page),
        0, 0 };                         /* unidirectional, render mode */

/* LQ hirez ImageWriter device descriptor */
const gx_device_admp far_data gs_iwlq_device = {
prn_device_margins_body(gx_device_admp, admp_initialize_device_procs, "iwlq",
        DEFAULT_WIDTH_10THS_US_LETTER,  /* width_10ths, 8.5" */
        DEFAULT_HEIGHT_10THS_US_LETTER, /* height_10ths, 11" */
        320, 216,                       /* X_DPI, Y_DPI */
        0.25, 0, 0.25, 0.25, 0.25, 0.25,/* origin and margins */
        1, 1, 1, 0, 2, 1,
        admp_print_page),
        0, 0 };                         /* unidirectional, render mode */

/* color LQ hirez ImageWriter device descriptor */
const gx_device_admp far_data gs_iwlqc_device = {
prn_device_margins_body(gx_device_admp, admp_initialize_device_procs_color, "iwlqc",
        DEFAULT_WIDTH_10THS_US_LETTER,  /* width_10ths, 8.5" */
        DEFAULT_HEIGHT_10THS_US_LETTER, /* height_10ths, 11" */
        320, 216,                       /* X_DPI, Y_DPI */
        0.25, 0, 0.25, 0.25, 0.25, 0.25,/* origin and margins */
        4, 4, 1, 1, 2, 2,               /* maxcomp, depth, maxgray, maxcolor, numgray, numcolor */
        admp_print_page),
        0, 0 };                         /* unidirectional, render mode */

static int
admp_get_params(gx_device* pdev, gs_param_list* plist)
{
        int code = gdev_prn_get_params(pdev, plist);

        if (code < 0 ||
                (code = param_write_bool(plist, "UNIDIRECTIONAL", &((gx_device_admp*)pdev)->unidirectional)) < 0 ||
                (code = param_write_int(plist, "RENDERMODE", &((gx_device_admp*)pdev)->rendermode)) < 0
                )
                return code;

        return code;
}

static int
admp_put_params(gx_device* pdev, gs_param_list* plist)
{
        int code = 0;
        bool unidirectional = ((gx_device_admp*)pdev)->unidirectional;
        int rendermode = ((gx_device_admp*)pdev)->rendermode;
        gs_param_name param_name;
        int ecode = 0;

        if ((code = param_read_bool(plist,
                (param_name = "UNIDIRECTIONAL"),
                &unidirectional)) < 0) {
                param_signal_error(plist, param_name, ecode = code);
        }
        if (ecode < 0)
                return code;

        if ((code = param_read_int(plist,
                (param_name = "RENDERMODE"),
                &rendermode)) < 0) {
                param_signal_error(plist, param_name, ecode = code);
        }
        if (ecode < 0)
                return code;

        ((gx_device_admp*)pdev)->unidirectional = unidirectional;
        ((gx_device_admp*)pdev)->rendermode = rendermode;

        return gdev_prn_put_params(pdev, plist);
}

/* Send the page to the printer output file. */
static int
admp_print_page(gx_device_printer* pdev, gp_file* gprn_stream)
{
        uint64_t code = gs_error_ok;
        char pcmd[255];
        int dev_rez;
        int dev_type;
        unsigned char color = 0;
        int line_size = gdev_mem_bytes_per_scan_line((gx_device*)pdev);
        int in_size = line_size * 8; /* Note that in_size is a multiple of 8 dots in height. */

        unsigned char color_order[4];

        gx_render_plane_t render_planes[4];

        byte *buf_in, *buf_out, *prn, *row;

        switch (pdev->color_info.num_components)
        {
        case 4:
                /* Use the YCMK order in an attempt to reduce cross-band contaminatien */
                color_order[0] = 2;
                color_order[1] = 0;
                color_order[2] = 1;
                color_order[3] = 3;
                break;
        case 1:
                color_order[0] = 0;
                break;
        default:
                outprintf(pdev->memory, ERRNUMCOMP);
                code = gs_note_error(gs_error_rangecheck);
                goto xit;
        }

        row = (byte*)gs_malloc(pdev->memory, line_size, 1, "admp_print_page(row)");
        buf_in = (byte*)gs_malloc(pdev->memory, in_size, 1, "admp_print_page(buf_in)");
        buf_out = (byte*)gs_malloc(pdev->memory, in_size, 1, "admp_print_page(buf_out)");
        prn  = (byte*)gs_malloc(pdev->memory,  in_size * 3, 1, "admp_print_page(prn)");
        byte* prn_saved = prn;

        byte* in = buf_in;
        byte* out = buf_out;
        int lnum = 0;

        /* Check allocations */
        if (row == 0 ||
            buf_in == 0 ||
            buf_out == 0 ||
            prn == 0)
        {
                errprintf(pdev->memory, ERRALLOC);
                code = gs_note_error(gs_error_VMerror);
                goto xit;
        }

        /* Determine device type */
        if (strcmp(pdev->dname, "iwlqc") == 0) dev_type = IWLQ;
        else if (strcmp(pdev->dname, "iwlq") == 0) dev_type = IWLQ;
        else if (strcmp(pdev->dname, "iwhic") == 0) dev_type = IWHI;
        else if (strcmp(pdev->dname, "iwhi") == 0) dev_type = IWHI;
        else if (strcmp(pdev->dname, "iwlo") == 0) dev_type = IWLO;
        else if (strcmp(pdev->dname, "appledmp") == 0) dev_type = DMP;
        else
        {
                errprintf(pdev->memory, ERRDEVTYPE);
                code = gs_note_error(gs_error_rangecheck);
                goto xit;
        }

        /* Initialize the printer by setting line-height and
         * print-head direction.
         */
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

        /* Select resolution */
        if (pdev->y_pixels_per_inch == 216) dev_rez = H320V216;
        else if (pdev->y_pixels_per_inch == 144) dev_rez = H160V144;
        else if (pdev->x_pixels_per_inch == 160) dev_rez = H160V72;
        else dev_rez = H120V72;

        /* Set resolution/character pitch */
        switch (dev_rez)
        {
        case H320V216:
                (void)strcpy(pcmd, ESC ELITEPROPORTIONAL ESC LQPROPORTIONAL);
                code = gp_fputs(pcmd, gprn_stream);
                break;
        case H160V144: /* falls through */
        case H160V72:
                (void)strcpy(pcmd, ESC ELITEPROPORTIONAL);
                code = gp_fputs(pcmd, gprn_stream);
                break;
        case H120V72: /* falls through */
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

        /* Pre-initialize the render planes */
        for (color = 0; color < pdev->color_info.num_components; ++color)
        {
                code = gx_render_plane_init(&render_planes[color_order[color]], (gx_device*)pdev, color_order[color]);

                if (code != 0)
                {
                        errprintf(pdev->memory, ERRPLANEINIT, code);
                        code = gs_note_error(gs_error_rangecheck);
                        goto xit;
                }
        }

        /* Zero the scanline buffer because the output file fills
           with patterns of 0xFF and 0x00 otherwise.  Additionally,
           not zeroing the row causes the 320x216 color IWLQ test
           case to hit ERRCMDLQ.  I speculate that there may be
           a padding/alingment issue at play but am not sure. */
        code = (uint64_t)memset(row, 0, line_size);

        if (code != (uint64_t)row)
        {
                errprintf(pdev->memory, ERRZEROROW, code);
                code = gs_note_error(gs_error_rangecheck);
                goto xit;
        }

        /* For each scan line in the main buffer */
        while (lnum < pdev->height)
        {
                byte* inp;
                byte* in_end;
                byte* out_end;
                byte* actual_row;
                int lcnt, ltmp;
                int count, passes;
                byte* prn_blk, * prn_end, * prn_tmp;
                uint actual_line_size;

                /* 8 raster tall bands per pass */
                switch (dev_rez)
                {
                case H320V216: passes = 3; break;
                case H160V144: passes = 2; break;
                case H160V72: /* falls through */
                case H120V72: /* falls through */
                default: passes = 1; break;
                }

                /* For each colorant, copy, transpose, copy and write a band of dots */
                for (color = 0; color < pdev->color_info.num_components; ++color)
                {
                        /* Get rendered scanlines from each plane and assemble into 8, 16, or 24-dot high strips */
                        for (count = 0; count < passes; count++)
                        {
                                /* For each line in an 8-dot high band*/
                                for (lcnt = 0; lcnt < 8; lcnt++)
                                {
                                        /* Select band count/height */
                                        switch (dev_rez)
                                        {
                                        case H320V216: ltmp = lcnt + 8 * count; break;
                                        case H160V144: ltmp = 2 * lcnt + count; break;
                                        case H160V72: /* falls through */
                                        case H120V72: /* falls through */
                                        default: ltmp = lcnt; break;
                                        }

                                        if ((lnum + ltmp) > pdev->height) /* if we're past end of page */
                                                /* write a blank line, ignoring return value */
                                                memset(in + lcnt * line_size , 0, line_size);
                                        else /* otherwise, get scan lines */
                                        {
                                                /* switch (4) */
                                                /* switch (pdev->color_info.num_components) */
                                                if (((gx_device_admp*)pdev)->rendermode == GPGL || pdev->color_info.num_components == 4)
                                                {
                                                        /* copy a scanline to the buffer */
                                                        code = (int)gdev_prn_get_lines(pdev,
                                                                lnum + ltmp,
                                                                1,
                                                                row,
                                                                line_size,
                                                                &actual_row,
                                                                &actual_line_size,
                                                                &render_planes[color_order[color]]);

                                                        if (code != 0 || actual_line_size != line_size)
                                                        {
                                                                errprintf(pdev->memory, ERRGPGL, code);
                                                                code = gs_note_error(gs_error_rangecheck);
                                                                goto xit;
                                                        }

                                                        /* The apple DMP printer seems to be odd in that the bit order on
                                                         * each line is reverse what might be expected.  Meaning, an
                                                         * underscore would be done as a series of 0x80, while on overscore
                                                         * would be done as a series of 0x01.  So we get each
                                                         * scan line in reverse order.
                                                         */

                                                        /* compute input buffer offset */
                                                        byte* in_start = in + line_size * (7 - lcnt);

                                                        /* copy row to input buffer */
                                                        code = (uint64_t)memcpy(in_start, row, line_size);

                                                        if (code != (uint64_t)in_start)
                                                        {
                                                                errprintf(pdev->memory, ERRMEMCOPY, code);
                                                                code = gs_note_error(gs_error_rangecheck);
                                                                goto xit;
                                                        }
                                                }
                                                else
                                                {
                                                        /* gdev_prn_copy_scan_lines(gx_device_printer * pdev, int y, byte * str, uint size) - gdevprn.c:1641 */
                                                        /* copy a scanline to the buffer */
                                                        code = gdev_prn_copy_scan_lines(
                                                                pdev,
                                                                (lnum + ltmp),
                                                                in + line_size * (7 - lcnt),
                                                                line_size);

                                                        if (code != 1)
                                                        {
                                                                errprintf(pdev->memory, ERRGPCSL, code, line_size);
                                                                code = gs_note_error(gs_error_rangecheck);
                                                                goto xit;
                                                        }

                                                }
                                        }
                                } /* for lcnt */

                                /* Here is the full input buffer.  It requires transposition
                                 * since the printer's native grapics format is 8-bits tall, with
                                 * a single bit midth.
                                 */
                                out_end = out;
                                inp = in;
                                in_end = inp + line_size;
                                for (; inp < in_end; inp++, out_end += 8)
                                {
                                        /* gdev_prn_transpose(...) returns void */
                                        gdev_prn_transpose_8x8(inp, line_size, out_end, 1);
                                }

                                /* Here is the output buffer, fully transposed */
                                out_end = out;

                                /* Determine tho size of the final print buffer. */
                                switch (dev_rez)
                                {
                                case H320V216: prn_end = prn + count; break;
                                case H160V144: prn_end = prn + in_size * count; break;
                                case H160V72: /* falls through */
                                case H120V72: /* falls through */
                                default: prn_end = prn; break;
                                }

                                /* Copy the output buffer to the final print buffer,
                                 * but don't use memcpy, possibly for some reason.
                                 */
                                while ((int)(out_end - out) < in_size)
                                {
                                        *prn_end = *(out_end++);
                                        if ((dev_rez) == H320V216) prn_end += 3;
                                        else prn_end++;
                                }
                        } /* for count */

                        /* Select ribbon color */
                        if (gx_device_has_color(pdev))
                        {
                                switch (color_order[color])
                                {
                                case 3: (void)strcpy(pcmd, ESC COLORSELECT BLACK); break;
                                case 2: (void)strcpy(pcmd, ESC COLORSELECT YELLOW); break;
                                case 1: (void)strcpy(pcmd, ESC COLORSELECT MAGENTA); break;
                                case 0: (void)strcpy(pcmd, ESC COLORSELECT CYAN); break;
                                }

                                code = gp_fputs(pcmd, gprn_stream);

                                if (code != strlen(pcmd))
                                {
                                        errprintf(pdev->memory, ERRCOLORSELECT);
                                        code = gs_note_error(gs_error_ioerror);
                                        goto xit;
                                }
                        }

                        /* Send bitmaps to printer, based on resolution setting

                        Note: each of the three vertical resolutions has a
                        unique and separate implementation, but follows the
                        same pattern as is documented in the H320V216
                        case and, therefore, is only documented in
                        comments there*.
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
                                                code = gp_fprintf(gprn_stream,
                                                        ESC BITMAPHIRPT "%04d%c%c%c",
                                                        (int)((prn_blk - prn) / 3),
                                                        //(int)(((prn_blk - prn) / 3) - (in_size)),
                                                        0, 0, 0);

                                                if (code != 9)
                                                {
                                                        errprintf(pdev->memory, ERRPOSLQ);
                                                        code = gs_note_error(gs_error_ioerror);
                                                        goto xit;
                                                }
                                        }
                                        else
                                                prn_blk = prn;

                                        /* send hirez bitmapped graphics mode command (with length parameter) */
                                        code = gp_fprintf(gprn_stream,
                                                ESC BITMAPHI "%04d",
                                                (int)((prn_end - prn_blk) / 3));

                                        if (code != 6)
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
                                                /* For V144, advance the page 1/144th of an inch for the first NLQ pass... */
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
                                /* then set the line-height for the remaining 15/144ths to finish the 2nd NLQ pass. */
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
                                prn_end = prn_blk + in_size;
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

                        if (pdev->color_info.num_components == 4 && color < 3)
                        {
                                /* Reset the print-head position for the next band */
                                code = gp_fputs(CR, gprn_stream);
                                if (code != 1)
                                {
                                        errprintf(pdev->memory, ERRCR);
                                        code = gs_note_error(gs_error_ioerror);
                                        goto xit;
                                }
                        }
                        else
                        {
                                /* Reset the print-head position and advance the page */
                                code = gp_fputs(CR LF, gprn_stream);
                                if (code != 2)
                                {
                                        errprintf(pdev->memory, ERRCRLF);
                                        code = gs_note_error(gs_error_ioerror);
                                        goto xit;
                                }
                        }
                } /* for color */

                /* Set lnum to reflect the number of 8-dot tall bands printed */
                switch (dev_rez)
                {
                case H320V216: lnum += 24; break;
                case H160V144: lnum += 16; break;
                case H160V72: /* falls through */
                case H120V72: /* falls through */
                default: lnum += 8; break;
                }
        } /* while lnum < pdev->height */

        /* Page should auto-eject since there were line-feeds for every
         * band -- no need for a form-feed.
         * 
         * Reset printer with factory defaults and elite character pitch.
         */
        code = gp_fputs(ESC BIDIRECTIONAL ESC LINE6PI ESC ELITE,
                gprn_stream);

        if (code != 6)
        {
                errprintf(pdev->memory, ERRRESET);
                code = gs_note_error(gs_error_ioerror);
                goto xit;
        }
        
        /* gp_fflush(...) returns void */
        gp_fflush(gprn_stream);

        code = gs_error_ok;

xit:
        /* gs_free returns void */
        gs_free(pdev->memory, (char*)prn, in_size * 3, 1, "admp_print_page(prn)");
        gs_free(pdev->memory, (char*)buf_out, in_size /* * 3 */, 1, "admp_print_page(buf_out)");
        gs_free(pdev->memory, (char*)buf_in, in_size, 1, "admp_print_page(buf_in)");
        gs_free(pdev->memory, (char*)row, line_size, 1, "admp_print_page(row)");

        return_error(code);
}