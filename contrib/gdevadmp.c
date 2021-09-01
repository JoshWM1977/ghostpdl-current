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

/* The device descriptors */
static dev_proc_print_page(dmp_print_page);

/* Standard DMP device */
const gx_device_printer far_data gs_appledmp_device =
prn_device(gdev_prn_initialize_device_procs_mono_bg, "appledmp",	/* The print_page proc is compatible with allowing bg printing */
        DEFAULT_WIDTH_10THS_US_LETTER,  /* width_10ths, 8.5" */
        DEFAULT_HEIGHT_10THS_US_LETTER, /* height_10ths, 11" */
        120, 72,                        /* X_DPI, Y_DPI */
        0.25, 0.25, 0.25, 0.25,         /* margins */
        1, dmp_print_page);

/*  lowrez Imagewriter device */
const gx_device_printer far_data gs_iwlo_device =
prn_device(gdev_prn_initialize_device_procs_mono_bg, "iwlo",	/* The print_page proc is compatible with allowing bg printing */
        DEFAULT_WIDTH_10THS_US_LETTER,  /* width_10ths, 8.5" */
        DEFAULT_HEIGHT_10THS_US_LETTER, /* height_10ths, 11" */
        160, 72,                        /* X_DPI, Y_DPI */
        0.25, 0.25, 0.25, 0.25,         /* margins */
        1, dmp_print_page);

/*  hirez Imagewriter device */
const gx_device_printer far_data gs_iwhi_device =
prn_device(gdev_prn_initialize_device_procs_mono_bg, "iwhi",	/* The print_page proc is compatible with allowing bg printing */
        DEFAULT_WIDTH_10THS_US_LETTER,  /* width_10ths, 8.5" */
        DEFAULT_HEIGHT_10THS_US_LETTER, /* height_10ths, 11" */
        160, 144,                       /* X_DPI, Y_DPI */
        0.25, 0.25, 0.25, 0.25,         /* margins */
        1, dmp_print_page);

/* LQ hirez Imagewriter device */
const gx_device_printer far_data gs_iwlq_device =
prn_device(gdev_prn_initialize_device_procs_mono_bg, "iwlq",	/* The print_page proc is compatible with allowing bg printing */
        DEFAULT_WIDTH_10THS_US_LETTER,  /* width_10ths, 8.5" */
        DEFAULT_HEIGHT_10THS_US_LETTER, /* height_10ths, 11" */
        320, 216,                       /* X_DPI, Y_DPI */
        0.25, 0.25, 0.25, 0.25,         /* margins */
        1, dmp_print_page);

/* Device type macros */
#define DMP 1
#define IWLO 2
#define IWHI 3
#define IWLQ 4

/* Device command macros */
#define ESC "\033"
#define CRLF "\r\n"
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

/* Error checking macro */
#define CHECKERR(call, expect, error) code = call; if (code expect) { code = gs_note_error(error); goto xit; } 

/* Send the page to the printer. */
static int
dmp_print_page(gx_device_printer *pdev, gp_file *gprn_stream)
{
        int code = gs_error_ok;
		
        int dev_type;

        int line_size = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
        /* Note that in_size is a multiple of 8. */
        int in_size = line_size * 8;

        byte *buf1 = (byte *)gs_malloc(pdev->memory, in_size, 1, "dmp_print_page(buf1)");
        byte* buf2 = (byte*)gs_malloc(pdev->memory, in_size, 1, "dmp_print_page(buf2)");
        byte *prn = (byte *)gs_malloc(pdev->memory, 3*in_size, 1, "dmp_print_page(prn)");

        byte *in = buf1;
        byte *out = buf2;
        int lnum = 0;

        /* Check allocations */
        if ( buf1 == 0 || buf2 == 0 || prn == 0 )
        {
                code = gs_note_error(gs_error_VMerror);
                goto xit;
        }

        if ( pdev->y_pixels_per_inch == 216 )
                dev_type = IWLQ;
        else if ( pdev->y_pixels_per_inch == 144 )
                dev_type = IWHI;
        else if ( pdev->x_pixels_per_inch == 160 )
                dev_type = IWLO;
        else
                dev_type = DMP;

        /* Initialize the printer. */

        CHECKERR(gp_fputs(CRLF ESC UNIDIRECTIONAL ESC LINEHEIGHT "16",
                        gprn_stream),
                != 8,
                gs_error_ioerror)

        switch(dev_type)
        {
        case IWLQ:
                CHECKERR(gp_fputs(ESC ELITEPROPORTIONAL ESC LQPROPORTIONAL,
                                gprn_stream),
                        != 5,
                        gs_error_ioerror)
                break;
        case IWHI:
        case IWLO:
                CHECKERR(gp_fputs(ESC ELITEPROPORTIONAL, gprn_stream),
                        != 2,
                        gs_error_ioerror)
                break;
        case DMP:
        default:
                CHECKERR(gp_fputs(ESC CONDENSED, gprn_stream),
                        != 2,
                        gs_error_ioerror)
                break;
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

/* The apple DMP printer seems to be odd in that the bit order on
 * each line is reverse what might be expected.  Meaning, an
 * underscore would be done as a series of 0x80, while on overscore
 * would be done as a series of 0x01.  So we get each
 * scan line in reverse order.
 */

                switch (dev_type)
                {
                case IWLQ: passes = 3; break;
                case IWHI: passes = 2; break;
                case IWLO:
                case DMP:
                default: passes = 1; break;
                }

                for (count = 0; count < passes; count++)
                {
                        for (lcnt=0; lcnt<8; lcnt++)
                        {
                                switch(dev_type)
                                {
                                case IWLQ: ltmp = lcnt + 8*count; break;
                                case IWHI: ltmp = 2*lcnt + count; break;
                                case IWLO:
                                case DMP:
                                default: ltmp = lcnt; break;
                                }

                                if ((lnum + ltmp) > pdev->height)
                                {
                                        CHECKERR((int)memset(in + lcnt * line_size,
                                                        0,
                                                        line_size),
                                                != (int)(in + lcnt * line_size),
                                                gs_error_undefined)
                                }
                                else
                                {
                                        CHECKERR((int)gdev_prn_copy_scan_lines(pdev,
                                                        lnum + ltmp,
                                                        in + line_size * (7 - lcnt),
                                                        line_size),
                                                != 1,
                                                gs_error_rangecheck)
                                }
                        }

                        out_end = out;
                        inp = in;
                        in_end = inp + line_size;
                        for ( ; inp < in_end; inp++, out_end += 8 )
                        {
                                /* gdev_prn_transpose(...) returns void */
                                gdev_prn_transpose_8x8(inp, line_size, out_end, 1);
                        }

                        out_end = out;

                        switch (dev_type)
                        {
                                case IWLQ: prn_end = prn + count; break;
                                case IWHI: prn_end = prn + in_size*count; break;
                                case IWLO:
                                case DMP:
                                default: prn_end = prn; break;
                        }

                        while ( (int)(out_end-out) < in_size)
                        {
                                *prn_end = *(out_end++);
                                if ((dev_type) == IWLQ) prn_end += 3;
                                else prn_end++;
                        }
                }

                switch (dev_type)
                {
                case IWLQ:
                        prn_blk = prn;
                        prn_end = prn_blk + in_size * 3;
                        while (prn_end > prn && prn_end[-1] == 0 &&
                                prn_end[-2] == 0 && prn_end[-3] == 0)
                        {
                                prn_end -= 3;
                        }
                        while (prn_blk < prn_end && prn_blk[0] == 0 &&
                                prn_blk[1] == 0 && prn_blk[2] == 0)
                        {
                                prn_blk += 3;
                        }
                        if (prn_end != prn_blk)
                        {
                                if ((prn_blk - prn) > 7)
                                {
                                        CHECKERR(gp_fprintf(gprn_stream,
                                                        ESC BITMAPHIRPT "%04d%c%c%c",
                                                        (int)((prn_blk - prn) / 3),
                                                        0, 0, 0),
                                                != 9,
                                                gs_error_ioerror)
                                }
                                else
                                        prn_blk = prn;
                                        
                                CHECKERR(gp_fprintf(gprn_stream,
                                                ESC BITMAPHI "%04d",
                                                (int)((prn_end - prn_blk) / 3)),
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
                case IWHI:
                        for (count = 0; count < 2; count++)
                        {
                                prn_blk = prn_tmp = prn + in_size*count;
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
                                        CHECKERR(gp_fputs(ESC LINEHEIGHT "01" CRLF,
                                                        gprn_stream),
                                                != 6,
                                                gs_error_ioerror)
                                }
                        }
                        CHECKERR(gp_fputs(ESC LINEHEIGHT "15", gprn_stream),
                                != 4,
                                gs_error_ioerror)
                        break;
                case IWLO:
                case DMP:
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

                CHECKERR(gp_fputs(CRLF, gprn_stream),
                        != 2,
                        gs_error_ioerror)

                switch (dev_type)
                {
                        case IWLQ: lnum += 24 ; break;
                        case IWHI: lnum += 16 ; break;
                        case IWLO:
                        case DMP:
                        default: lnum += 8 ; break;
                }
        }

        /* ImageWriter will skip a whole page if too close to end */
        /* so skip back more than an inch */
        if ( !(dev_type == DMP) )
                CHECKERR(gp_fputs(ESC LINEHEIGHT "99" LF LF ESC LINEFEEDREV LF LF LF LF ESC LINEFEEDFWD,
                                gprn_stream),
                        != 14,
                        gs_error_ioerror)

        /* Formfeed and Reset printer */
        CHECKERR(gp_fputs(ESC LINEHEIGHT "16" FF ESC BIDIRECTIONAL ESC LINE8PI ESC ELITE,
                        gprn_stream),
                != 11,
                gs_error_ioerror)

        /* gp_fflush(...) returns void */
        gp_fflush(gprn_stream);

        code = gs_error_ok;

        xit:
        gs_free(pdev->memory, (char *)prn, in_size, 1, "dmp_print_page(prn)");
        gs_free(pdev->memory, (char *)buf2, in_size, 1, "dmp_print_page(buf2)");
        gs_free(pdev->memory, (char*)buf1, in_size, 1, "dmp_print_page(buf1)");
        return_error(code);
}
