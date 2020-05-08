/* Copyright (C) 2001-2020 Artifex Software, Inc.
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


#ifndef plparams_h_INCLUDED
#define plparams_h_INCLUDED

/* plparams.h */

int pcl_pjl_pdfmark(gs_memory_t *mem, gx_device *device, char *pdfmark);
int pcl_pjl_setdistillerparams(gs_memory_t *mem, gx_device *device, char *distillerparams);

#endif
