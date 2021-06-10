/* Copyright (C) 2020-2021 Artifex Software, Inc.
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

/* code for CIDFontType2/Type 9 font handling */
/* CIDFonts with Truetype outlines */

#include "pdf_int.h"
#include "pdf_font.h"
#include "pdf_font0.h"
#include "pdf_fontTT.h"
#include "pdf_font_types.h"
#include "pdf_stack.h"
#include "pdf_file.h"
#include "pdf_dict.h"
#include "pdf_array.h"
#include "pdf_deref.h"
#include "gxfont42.h"
#include "gxfcid.h"

static int
pdfi_cidtype2_string_proc(gs_font_type42 * pfont, ulong offset, uint length,
                  const byte ** pdata)
{
    pdf_cidfont_type2 *ttfont = (pdf_cidfont_type2 *)pfont->client_data;
    int code = 0;

    if (offset + length > ttfont->sfnt.size) {
        *pdata = NULL;
        code = gs_note_error(gs_error_invalidfont);
    }
    else {
        *pdata = ttfont->sfnt.data + offset;
    }
    return code;
}

static int
pdfi_cidtype2_CIDMap_proc(gs_font_cid2 *pfont, gs_glyph glyph)
{
    return (int)(glyph - GS_MIN_CID_GLYPH);
}

/* This should replace the glyph metrics with the ones from the PDF */
static int
pdfi_cidtype2_glyph_info(gs_font *font, gs_glyph glyph, const gs_matrix *pmat,
                     int members, gs_glyph_info_t *info) {
    int code;
    pdf_cidfont_type2 *pdffont11 = (pdf_cidfont_type2 *)font->client_data;
    code = (*pdffont11->orig_glyph_info)(font, glyph, pmat, members, info);

    return code;
}

static int
pdfi_alloc_cidtype2_font(pdf_context *ctx, pdf_cidfont_type2 **font, bool is_cid)
{
    pdf_cidfont_type2 *ttfont = NULL;
    gs_font_cid2 *pfont = NULL;

    ttfont = (pdf_cidfont_type2 *)gs_alloc_bytes(ctx->memory, sizeof(pdf_cidfont_type2), "pdfi (cidtype2 pdf_font)");
    if (ttfont == NULL)
        return_error(gs_error_VMerror);

    memset(ttfont, 0x00, sizeof(pdf_cidfont_type2));
    ttfont->type = PDF_FONT;
    ttfont->ctx = ctx;
    ttfont->pdfi_font_type = e_pdf_cidfont_type2;

#if REFCNT_DEBUG
    ttfont->UID = ctx->UID++;
    dmprintf2(ctx->memory, "Allocated object of type %c with UID %"PRIi64"\n", ttfont->type, ttfont->UID);
#endif

    pdfi_countup(ttfont);

    pfont = (gs_font_cid2 *)gs_alloc_struct(ctx->memory, gs_font_cid2, &st_gs_font_cid2,
                            "pdfi (cidtype2 pfont)");
    if (pfont == NULL) {
        pdfi_countdown(ttfont);
        return_error(gs_error_VMerror);
    }
    memset(pfont, 0x00, sizeof(gs_font_cid2));

    ttfont->pfont = (gs_font_base *)pfont;

    gs_make_identity(&pfont->orig_FontMatrix);
    gs_make_identity(&pfont->FontMatrix);
    pfont->next = pfont->prev = 0;
    pfont->memory = ctx->memory;
    pfont->dir = ctx->font_dir;
    pfont->is_resource = false;
    gs_notify_init(&pfont->notify_list, ctx->memory);
    pfont->base = (gs_font *) ttfont->pfont;
    pfont->client_data = ttfont;
    pfont->WMode = 0;
    pfont->PaintType = 0;
    pfont->StrokeWidth = 0;
    pfont->is_cached = 0;
    pfont->FAPI = NULL;
    pfont->FAPI_font_data = NULL;
    pfont->procs.init_fstack = gs_default_init_fstack;
    pfont->procs.next_char_glyph = gs_default_next_char_glyph;
    pfont->FontType = ft_CID_TrueType;
    pfont->ExactSize = fbit_use_outlines;
    pfont->InBetweenSize = fbit_use_outlines;
    pfont->TransformedChar = fbit_use_outlines;
    /* We may want to do something clever with an XUID here */
    pfont->id = gs_next_ids(ctx->memory, 1);
    uid_set_UniqueID(&pfont->UID, pfont->id);
    /* The buildchar proc will be filled in by FAPI -
       we won't worry about working without FAPI */
    pfont->procs.encode_char = pdfi_encode_char;
    pfont->data.string_proc = pdfi_cidtype2_string_proc;
    pfont->procs.glyph_name = pdfi_glyph_name;
    pfont->procs.decode_glyph = pdfi_decode_glyph;
    pfont->procs.define_font = gs_no_define_font;
    pfont->procs.make_font = gs_no_make_font;
    pfont->procs.font_info = gs_default_font_info;
    pfont->procs.glyph_info = gs_default_glyph_info;
    pfont->procs.glyph_outline = gs_no_glyph_outline;
    pfont->procs.build_char = NULL;
    pfont->procs.same_font = gs_default_same_font;
    pfont->procs.enumerate_glyph = gs_no_enumerate_glyph;

    pfont->encoding_index = 1;          /****** WRONG ******/
    pfont->nearest_encoding_index = 1;          /****** WRONG ******/

    cid_system_info_set_null(&pfont->cidata.common.CIDSystemInfo);
    pfont->cidata.common.CIDCount = 0; /* set later */
    pfont->cidata.common.GDBytes = 2; /* not used */
    pfont->cidata.MetricsCount = 0;
    pfont->cidata.CIDMap_proc = pdfi_cidtype2_CIDMap_proc;

    pfont->client_data = (void *)ttfont;

    *font = ttfont;
    return 0;
}

int pdfi_read_cidtype2_font(pdf_context *ctx, pdf_dict *font_dict, pdf_dict *stream_dict, pdf_dict *page_dict, byte *buf, int64_t buflen, pdf_font **ppfont)
{
    pdf_cidfont_type2 *font;
    int code = 0;
    pdf_obj *fontdesc = NULL;
    pdf_obj *obj = NULL;

    *ppfont = NULL;

    code = pdfi_dict_knownget_type(ctx, font_dict, "FontDescriptor", PDF_DICT, &fontdesc);
    if (code <= 0) {
        /* We own the buffer now, so we must free it on error */
        gs_free_object(ctx->memory, buf, "pdfi_read_cidtype2_font");
        return_error(gs_error_invalidfont);
    }

    if ((code = pdfi_alloc_cidtype2_font(ctx, &font, false)) < 0) {
        /* We own the buffer now, so we must free it on error */
        gs_free_object(ctx->memory, buf, "pdfi_read_cidtype2_font");
        pdfi_countdown(fontdesc);
        return code;
    }
    font->PDF_font = font_dict;
    pdfi_countup(font_dict);
    font->FontDescriptor = (pdf_dict *)fontdesc;
    fontdesc = NULL;

    /* Ownership of buf is now part of the font and managed via its lifetime */
    font->sfnt.data = buf;
    font->sfnt.size = buflen;

    /* Strictly speaking BaseFont is required, but we can continue without one */
    code = pdfi_dict_knownget_type(ctx, font_dict, "BaseFont", PDF_NAME, (pdf_obj **)&obj);
    if (code > 0) {
        pdf_name *nobj = (pdf_name *)obj;
        int nlen = nobj->length > gs_font_name_max ? gs_font_name_max : nobj->length;

        memcpy(font->pfont->key_name.chars, nobj->data, nlen);
        font->pfont->key_name.chars[nlen] = 0;
        font->pfont->key_name.size = nlen;
        memcpy(font->pfont->font_name.chars, nobj->data, nlen);
        font->pfont->font_name.chars[nlen] = 0;
        font->pfont->font_name.size = nlen;
        pdfi_countdown(obj);
        obj = NULL;
    }

    code = pdfi_dict_knownget_type(ctx, font_dict, "DW", PDF_INT, (pdf_obj **)&obj);
    if (code > 0) {
        font->DW = ((pdf_num *)obj)->value.i;
        pdfi_countdown(obj);
        obj = NULL;
    }
    else {
        font->DW = 1000;
    }
    code = pdfi_dict_knownget_type(ctx, font_dict, "DW2", PDF_ARRAY, (pdf_obj **)&obj);
    if (code > 0) {
        font->DW2 = (pdf_array *)obj;
        obj = NULL;
    }
    else {
        font->DW2 = NULL;
    }
    code = pdfi_dict_knownget_type(ctx, font_dict, "W", PDF_ARRAY, (pdf_obj **)&obj);
    if (code > 0) {
        font->W = (pdf_array *)obj;
        obj = NULL;
    }
    else {
        font->W = NULL;
    }
    code = pdfi_dict_knownget_type(ctx, font_dict, "W2", PDF_ARRAY, (pdf_obj **)&obj);
    if (code > 0) {
        font->W2 = (pdf_array *)obj;
        obj = NULL;
    }
    else {
        font->W2 = NULL;
    }

    code = pdfi_dict_knownget(ctx, font_dict, "CIDToGIDMap", (pdf_obj **)&obj);
    if (code > 0) {
        font->cidtogidmap.data = NULL;
        font->cidtogidmap.size = 0;
        /* CIDToGIDMap can only be a stream or a name, and if it's a name
           it's only permitted to be "/Identity", so ignore it
         */
        if (obj->type == PDF_STREAM) {
            code = pdfi_stream_to_buffer(ctx, (pdf_stream *)obj,
                        &(font->cidtogidmap.data),
                        (int64_t *)&(font->cidtogidmap.size));
        }
        pdfi_countdown(obj);
        obj = NULL;
    }

    code = gs_type42_font_init((gs_font_type42 *)font->pfont, 0);
    if (code < 0) {
        goto error;
    }
    font->orig_glyph_info = font->pfont->procs.glyph_info;
    font->pfont->procs.glyph_info = pdfi_cidtype2_glyph_info;

    ((gs_font_type42 *)font->pfont)->data.substitute_glyph_index_vertical = gs_type42_substitute_glyph_index_vertical;

    code = gs_definefont(ctx->font_dir, (gs_font *)font->pfont);
    if (code < 0) {
        goto error;
    }

    code = pdfi_fapi_passfont((pdf_font *)font, 0, NULL, NULL, font->sfnt.data, font->sfnt.size);
    if (code < 0) {
        goto error;
    }

    /* object_num can be zero if the dictionary was defined inline */
    if (font->object_num != 0) {
        code = replace_cache_entry(ctx, (pdf_obj *)font);
        if (code < 0)
            goto error;
    }

    if (font)
        *ppfont = (pdf_font *)font;
    return code;
error:

    pdfi_countdown(obj);
    pdfi_countdown(font);
    return code;
}

int pdfi_free_font_cidtype2(pdf_obj *font)
{
    pdf_cidfont_type2 *pdfcidf = (pdf_cidfont_type2 *)font;
    gs_font_cid2 *pfont = (gs_font_cid2 *)pdfcidf->pfont;
    gs_free_object(OBJ_MEMORY(pdfcidf), pfont, "pdfi_free_font_cidtype2(pfont)");

    gs_free_object(OBJ_MEMORY(pdfcidf), pdfcidf->cidtogidmap.data, "pdfi_free_font_cidtype2(cidtogidmap.data)");
    gs_free_object(OBJ_MEMORY(pdfcidf), pdfcidf->sfnt.data, "pdfi_free_font_cidtype2(sfnt.data)");

    pdfi_countdown(pdfcidf->PDF_font);
    pdfi_countdown(pdfcidf->BaseFont);
    pdfi_countdown(pdfcidf->FontDescriptor);
    pdfi_countdown(pdfcidf->W);
    pdfi_countdown(pdfcidf->DW2);
    pdfi_countdown(pdfcidf->W2);

    gs_free_object(OBJ_MEMORY(pdfcidf), pdfcidf, "pdfi_free_font_cidtype2(pdfcidf)");
return 0;

    return 0;
}
