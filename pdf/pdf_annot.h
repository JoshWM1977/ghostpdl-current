/* Copyright (C) 2019-2021 Artifex Software, Inc.
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

/* Annotation handling for the PDF interpreter */

#ifndef PDF_ANNOTATIONS
#define PDF_ANNOTATIONS

int pdfi_do_annotations(pdf_context *ctx, pdf_dict *page_dict);
int pdfi_do_acroform(pdf_context *ctx, pdf_dict *page_dict);

#endif
