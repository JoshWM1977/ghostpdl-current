/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pxink.c */
/* PCL XL ink setting operators */

#include "math_.h"
#include "stdio_.h"			/* for NULL */
#include "gstypes.h"
#include "gsmemory.h"
#include "pxoper.h"
#include "pxstate.h"
#include "gxarith.h"
#include "gsstate.h"
#include "gxcspace.h"			/* must precede gscolor2.h */
#include "gscolor2.h"
#include "gscoord.h"
#include "gsimage.h"
#include "gspath.h"
#include "gxdevice.h"
#include "gxht.h"
#include "gxstate.h"

/*
 * Contrary to the documentation, SetColorSpace apparently doesn't set the
 * brush or pen to black.  To produce this behavior, uncomment the
 * following #define.
 */
#define SET_COLOR_SPACE_NO_SET_BLACK

/* Forward references */
private int px_set_default_screen(P3(px_state_t *pxs, int method,
				     const gs_point *origin));

/* ---------------- Utilities ---------------- */

/* ------ Halftones ------ */

/* Define a transfer function without gamma correction. */
private float
identity_transfer(floatp tint, const gx_transfer_map *ignore_map)
{	return tint;
}

/* Set the default halftone screen. */
static const byte order16x16[256] = {
#if 0
  /*
   * The following is a standard 16x16 ordered dither, except that
   * the very last pass goes in the order (0,1,2,3) rather than
   * (0,3,1,2).  This leads to excessive cancellation when the source
   * and paint halftones interact, but it's better than the standard
   * order, which has inadequate cancellation. 
   *
   * This matrix is generated by the following call:
   *	[ <00 88 08 80> <00 44 04 40> <00 22 02 20> <00 01 10 11> ]
   *	{ } makedither
   */
 0,64,32,96,8,72,40,104,2,66,34,98,10,74,42,106,
 128,192,160,224,136,200,168,232,130,194,162,226,138,202,170,234,
 48,112,16,80,56,120,24,88,50,114,18,82,58,122,26,90,
 176,240,144,208,184,248,152,216,178,242,146,210,186,250,154,218,
 12,76,44,108,4,68,36,100,14,78,46,110,6,70,38,102,
 140,204,172,236,132,196,164,228,142,206,174,238,134,198,166,230,
 60,124,28,92,52,116,20,84,62,126,30,94,54,118,22,86,
 188,252,156,220,180,244,148,212,190,254,158,222,182,246,150,214,
 3,67,35,99,11,75,43,107,1,65,33,97,9,73,41,105,
 131,195,163,227,139,203,171,235,129,193,161,225,137,201,169,233,
 51,115,19,83,59,123,27,91,49,113,17,81,57,121,25,89,
 179,243,147,211,187,251,155,219,177,241,145,209,185,249,153,217,
 15,79,47,111,7,71,39,103,13,77,45,109,5,69,37,101,
 143,207,175,239,135,199,167,231,141,205,173,237,133,197,165,229,
 63,127,31,95,55,119,23,87,61,125,29,93,53,117,21,85,
 191,255,159,223,183,247,151,215,189,253,157,221,181,245,149,213
#  define source_phase_x 1
#  define source_phase_y 1
#else
/*
 * The following is a 45 degree spot screen with the spots enumerated
 * in a defined order.  This matrix is generated by the following call:

/gamma_transfer {
  dup dup 0 le exch 1 ge or not {
    dup 0.17 lt
     { 3 mul }
     { dup 0.35 lt { 0.78 mul 0.38 add } { 0.53 mul 0.47 add } ifelse }
   ifelse
  } if
} def

[ [0 136 8 128 68 204 76 196]
  [18 33 17 34   1 2 19 35 50 49 32 16   3 48 0 51
   -15 -14 20 36 66 65 47 31   -13 64 15 52   -30 81 30 37]
] /gamma_transfer load makedither

 */
 38,11,14,32,165,105,90,171,38,12,14,33,161,101,88,167,
 30,6,0,16,61,225,231,125,30,6,1,17,63,222,227,122,
 27,3,8,19,71,242,205,110,28,4,9,20,74,246,208,106,
 35,24,22,40,182,46,56,144,36,25,22,41,186,48,58,148,
 152,91,81,174,39,12,15,34,156,95,84,178,40,13,16,34,
 69,212,235,129,31,7,2,18,66,216,239,133,32,8,2,18,
 79,254,203,114,28,4,10,20,76,250,199,118,29,5,10,21,
 193,44,54,142,36,26,23,42,189,43,52,139,37,26,24,42,
 39,12,15,33,159,99,87,169,38,11,14,33,163,103,89,172,
 31,7,1,17,65,220,229,123,30,6,1,17,62,223,233,127,
 28,4,9,20,75,248,210,108,27,3,9,19,72,244,206,112,
 36,25,23,41,188,49,60,150,35,25,22,41,184,47,57,146,
 157,97,85,180,40,13,16,35,154,93,83,176,39,13,15,34,
 67,218,240,135,32,8,3,19,70,214,237,131,31,7,2,18,
 78,252,197,120,29,5,11,21,80,255,201,116,29,5,10,21,
 191,43,51,137,37,27,24,43,195,44,53,140,37,26,23,42
#  define source_phase_x 4
#  define source_phase_y 0
#endif
};
private int
px_set_default_screen(px_state_t *pxs, int method, const gs_point *origin)
{	gs_state *pgs = pxs->pgs;
	int px = (int)origin->x, py = (int)origin->y;
	gs_halftone ht;
	int code;

	gs_settransfer(pgs, identity_transfer);
	ht.type = ht_type_threshold;
	ht.params.threshold.width = ht.params.threshold.height = 16;
	ht.params.threshold.thresholds.data = order16x16;
	ht.params.threshold.thresholds.size = 256;
	ht.params.threshold.transfer = 0;
	ht.params.threshold.transfer_closure.proc = 0;
	code = gs_sethalftone(pgs, &ht);
	if ( code < 0 )
	  return code;
	code = gs_sethalftonephase(pgs, px, py);
	/*
	 * Here is where we do the dreadful thing that appears to be
	 * necessary to match the observed behavior of LaserJet 5 and
	 * 6 printers with respect to superimposing halftoned source
	 * and pattern.
	 */
	if ( code < 0 )
	  return code;
	return gs_setscreenphase(pgs, px + source_phase_x,
				 py + source_phase_y, gs_color_select_source);
}

/* Set the size for a default halftone screen. */
private void
px_set_default_screen_size(px_state_t *pxs, int method)
{	px_gstate_t *pxgs = pxs->pxgs;

	pxgs->halftone.width = pxgs->halftone.height = 16;
}

/* If necessary, set the halftone in the graphics state. */
int
px_set_halftone(px_state_t *pxs)
{	px_gstate_t *pxgs = pxs->pxgs;
	int code;

	if ( pxgs->halftone.set )
	  return 0;
	if ( pxgs->halftone.method != eDownloaded )
	  code = px_set_default_screen(pxs, pxgs->halftone.method,
				       &pxgs->halftone.origin);
	else
	  { gs_state *pgs = pxs->pgs;
	    gs_halftone ht;

	    ht.type = ht_type_threshold;
	    switch ( pxs->orientation )
	      {
	      case ePortraitOrientation:
	      case eReversePortrait:
		ht.params.threshold.width = pxgs->halftone.width;
		ht.params.threshold.height = pxgs->halftone.height;
		break;
	      case eLandscapeOrientation:
	      case eReverseLandscape:
		ht.params.threshold.width = pxgs->halftone.height;
		ht.params.threshold.height = pxgs->halftone.width;
		break;
	      }
	    /* Stupid C compilers don't allow structure assignment where */
	    /* the only incompatibility is an assignment of a T * to a */
	    /* const T * (which *is* allowed as a simple assignment): */
	    ht.params.threshold.thresholds.data =
	      pxgs->halftone.thresholds.data;
	    ht.params.threshold.thresholds.size =
	      pxgs->halftone.thresholds.size;
	    /* Downloaded dither matrices disable the transfer function. */
	    ht.params.threshold.transfer = identity_transfer;
	    code = gs_sethalftone(pgs, &ht);
	    if ( code >= 0 )
	      code = gs_sethalftonephase(pgs,
					 (int)pxgs->halftone.origin.x,
					 (int)pxgs->halftone.origin.y);
	    if ( code < 0 )
	      gs_free_string(pxs->memory, pxgs->halftone.thresholds.data,
			     pxgs->halftone.thresholds.size,
			     "px_set_halftone(thresholds)");
	    else
	      { gs_free_string(pxs->memory, pxgs->dither_matrix.data,
			       pxgs->dither_matrix.size,
			       "px_set_halftone(dither_matrix)");
	        pxgs->dither_matrix = pxgs->halftone.thresholds;
	      }
	    pxgs->halftone.thresholds.data = 0;
	    pxgs->halftone.thresholds.size = 0;
	  }
	if ( code < 0 )
	  return code;
	pxgs->halftone.set = true;
	/* Cached patterns have already been halftoned, so clear the cache. */
	px_purge_pattern_cache(pxs, eSessionPattern);
	return 0;
}

/* ------ Patterns ------ */

/*
 * The library caches patterns in their fully rendered form, i.e., after
 * halftoning.  In order to avoid seams or anomalies, we have to replicate
 * the pattern so that its size is an exact multiple of the halftone size.
 */

private uint
ilcm(uint x, uint y)
{	return x * (y / igcd(x, y));
}

/* Render a pattern. */
private int
px_paint_pattern(const gs_client_color *pcc, gs_state *pgs)
{	const gs_client_pattern *ppat = gs_getpattern(pcc);
	const px_pattern_t *pattern = ppat->client_data;
	const byte *dp = pattern->data;
	gs_image_enum *penum;
	gs_color_space color_space;
	gs_image_t image;
	int code;
	int num_components =
	  (pattern->params.indexed || pattern->params.color_space == eGray ?
	   1 : 3);
	uint rep_width = pattern->params.width;
	uint rep_height = pattern->params.height;
	uint full_width = (uint)ppat->XStep;
	uint full_height = (uint)ppat->YStep;
	uint bits_per_row, bytes_per_row;
	int x;

	code = px_image_color_space(&color_space, &image, &pattern->params,
				    &pattern->palette, pgs);
	if ( code < 0 )
	  return code;
	penum = gs_image_enum_alloc(gs_state_memory(pgs), "px_paint_pattern");
	if ( penum == 0 )
	  return_error(errorInsufficientMemory);
	bits_per_row = rep_width * image.BitsPerComponent * num_components;
	bytes_per_row = (bits_per_row + 7) >> 3;
	/*
	 * As noted above, in general, we have to replicate the original
	 * pattern to a multiple that avoids halftone seams.  If the
	 * number of bits per row is a multiple of 8, we can do this with
	 * a single image; otherwise, we need one image per X replica.
	 * To simplify the code, we always use the (slightly) slower method.
	 */
	image.Width = rep_width;
	image.Height = full_height;
	for ( x = 0; x < full_width; x += rep_width )
	  { int y;

	    image.ImageMatrix.tx = -x;
	    code = gs_image_init(penum, &image, false, pgs);
	    if ( code < 0 )
	      break;
	    for ( y = 0; code >= 0 && y < full_height; ++y )
	      { const byte *row = dp + (y % rep_height) * bytes_per_row;
	        uint used;

		code = gs_image_next(penum, row, bytes_per_row, &used);
	      }
	    gs_image_cleanup(penum);
	  }
	gs_free_object(gs_state_memory(pgs), penum, "px_paint_pattern");
	return code;
}

/* Create the rendering of a pattern. */
private int
render_pattern(gs_client_color *pcc, const px_pattern_t *pattern,
  const px_value_t *porigin, const px_value_t *pdsize, px_state_t *pxs)
{	px_gstate_t *pxgs = pxs->pxgs;
	uint rep_width = pattern->params.width;
	uint rep_height = pattern->params.height;
	uint full_width, full_height;
	gs_state *pgs = pxs->pgs;
	gs_client_pattern template;

	/*
	 * If halftoning may occur, replicate the pattern so we don't get
	 * halftone seams.
	 */
	{ gx_device *dev = gs_currentdevice(pgs);
	  if ( dev->color_info.max_gray > 31 &&
	       (dev->color_info.num_components == 1 ||
		dev->color_info.max_color > 31)
	     )
	    { /* No halftoning. */
	      full_width = rep_width;
	      full_height = rep_height;
	    }
	  else
	    { full_width = ilcm(rep_width, pxgs->halftone.width);
	      full_height = ilcm(rep_height, pxgs->halftone.height);
	      /*
	       * If the pattern would be enormous, don't replicate it.
	       * This is a HACK.
	       */
	      if ( full_width > 10000 )
		full_width = rep_width;
	      if ( full_height > 10000 )
		full_height = rep_height;
	    }
	}
	/* Construct a Pattern for the library, and render it. */
	uid_set_UniqueID(&template.uid, pattern->id);
	template.PaintType = 1;
	template.TilingType = 1;
	template.BBox.p.x = 0;
	template.BBox.p.y = 0;
	template.BBox.q.x = full_width;
	template.BBox.q.y = full_height;
	template.XStep = full_width;
	template.YStep = full_height;
	template.PaintProc = px_paint_pattern;
	template.client_data = pattern;
	{ gs_matrix mat;
	  gs_point dsize;
	  int code;
	  
	  if ( porigin )
	    gs_make_translation(real_value(porigin, 0),
				real_value(porigin, 1), &mat);
	  else
	    gs_make_identity(&mat);
	  if ( pdsize )
	    { dsize.x = real_value(pdsize, 0);
	      dsize.y = real_value(pdsize, 1);
	    }
	  else
	    { dsize.x = pattern->params.dest_width;
	      dsize.y = pattern->params.dest_height;
	    }
	  gs_matrix_scale(&mat, dsize.x / rep_width, dsize.y / rep_height,
			  &mat);
	  /*
	   * gs_makepattern will make a copy of the current gstate.
	   * We don't want this copy to contain any circular back pointers
	   * to px_pattern_ts: such pointers are unnecessary, because
	   * px_paint_pattern doesn't use the pen and brush (in fact,
	   * it doesn't even reference the px_gstate_t).  We also want to
	   * reset the path and clip path.  The easiest (although not by
	   * any means the most efficient) way to do this is to do a gsave,
	   * reset the necessary things, do the makepattern, and then do
	   * a grestore.
	   */
	  code = gs_gsave(pgs);
	  if ( code < 0 )
	    return code;
	  { px_gstate_t *pxgs = pxs->pxgs;

	    px_gstate_rc_adjust(pxgs, -1, pxgs->memory);
	    pxgs->brush.type = pxgs->pen.type = pxpNull;
	    gs_newpath(pgs);
	    gs_initclip(pgs);
	  }
	  code = gs_makepattern(pcc, &template, &mat, pgs, NULL);
	  gs_grestore(pgs);
	  return code;
	}
}

/* ------ Brush/pen ------ */

/* Check parameters and execute SetBrushSource or SetPenSource: */
/* pxaRGBColor, pxaGrayLevel, pxaNullBrush/Pen, pxaPatternSelectID, */
/* pxaPatternOrigin, pxaNewDestinationSize */
private ulong near
int_type_max(px_data_type_t type)
{	return
	  (type & pxd_ubyte ? 255 :
	   type & pxd_uint16 ? 65535 :
	   type & pxd_sint16 ? 32767 :
	   type & pxd_uint32 ? (ulong)0xffffffff :
	   /* type & pxd_sint32 */ 0x7fffffff);
}
private real near
fraction_value(const px_value_t *pv, int i)
{	px_data_type_t type = pv->type;
	real v;

	if ( type & pxd_real32 )
	  return pv->value.ra[i];
	v = pv->value.ia[i];
	return (v < 0 ? 0 : v / int_type_max(type));
}
private int near
set_source(const px_args_t *par, px_state_t *pxs, px_paint_t *ppt)
{	px_gstate_t *pxgs = pxs->pxgs;
	int code = 0;

	if ( par->pv[3] )	/* pxaPatternSelectID */
	  { px_value_t key;
	    void *value;
	    px_pattern_t *pattern;
	    gs_client_color ccolor;
	    int code;

	    if ( par->pv[0] || par->pv[1] || par->pv[2] )
	      return_error(errorIllegalAttributeCombination);
	    key.type = pxd_array | pxd_ubyte;
	    key.value.array.data = (byte *)&par->pv[3]->value.i;
	    key.value.array.size = sizeof(integer);
	    if ( !(px_dict_find(&pxgs->temp_pattern_dict, &key, &value) ||
		   px_dict_find(&pxs->page_pattern_dict, &key, &value) ||
		   px_dict_find(&pxs->session_pattern_dict, &key, &value))
	       )
	      return_error(errorRasterPatternUndefined);
	    pattern = value;
	    code = render_pattern(&ccolor, pattern, par->pv[4], par->pv[5],
				  pxs);
	    /*
	     * We don't use px_paint_rc_adjust(... 1 ...) here, because
	     * gs_makepattern creates pattern instances with a reference
	     * count already set to 1.
	     */
	    rc_increment(pattern);
	    if ( code < 0 )
	      return code;
	    px_paint_rc_adjust(ppt, -1, pxs->memory);
	    ppt->type = pxpPattern;
	    ppt->value.pattern.pattern = pattern;
	    ppt->value.pattern.color = ccolor;
	    ppt->needs_halftone = true;
	  }
	else if ( par->pv[4] || par->pv[5] )
	  return_error(errorIllegalAttributeCombination);
	else if ( par->pv[0] )	/* pxaRGBColor */
	  { const px_value_t *prgb = par->pv[0];
	    uint i;

	    if ( par->pv[1] || par->pv[2] )
	      return_error(errorIllegalAttributeCombination);
	    if ( pxgs->color_space != eRGB )
	      return_error(errorColorSpaceMismatch);
	    px_paint_rc_adjust(ppt, -1, pxs->memory);
	    ppt->type = pxpRGB;
	    for ( i = 0; i < 3; ++i )
	      if ( prgb->type & pxd_any_real )
		ppt->value.rgb[i] = real_elt(prgb, i);
	      else
		{ integer v = integer_elt(prgb, i);
		  ppt->value.rgb[i] =
		    (v < 0 ? 0 : (real)v / int_type_max(prgb->type));
		}
#define rgb_is(v)\
  (ppt->value.rgb[0] == v && ppt->value.rgb[1] == v && ppt->value.rgb[2] == v)
	    ppt->needs_halftone = !(rgb_is(0) || rgb_is(1));
#undef rgb_is
	  }
	else if ( par->pv[1] )	/* pxaGrayLevel */
	  { if ( par->pv[2] )
	      return_error(errorIllegalAttributeCombination);
	    if ( pxgs->color_space != eGray )
	      return_error(errorColorSpaceMismatch);
	    px_paint_rc_adjust(ppt, -1, pxs->memory);
	    ppt->type = pxpGray;
	    ppt->value.gray = fraction_value(par->pv[1], 0);
	    ppt->needs_halftone = ppt->value.gray != 0 && ppt->value.gray != 1;
	  }
	else if ( par->pv[2] )	/* pxaNullBrush/Pen */
	  { px_paint_rc_adjust(ppt, -1, pxs->memory);
	    ppt->type = pxpNull;
	    ppt->needs_halftone = false;
	  }
	else
	  return_error(errorMissingAttribute);
	/*
	 * Update the halftone to the most recently set one.
	 * This will do the wrong thing if we set the brush or pen source,
	 * set the halftone, and then set the other source, but we have
	 * no way to handle this properly with the current library.
	 */
	if ( code >= 0 && ppt->needs_halftone )
	  code = px_set_halftone(pxs);
	return code;
}

/* Set up a brush or pen for drawing. */
/* If it is a pattern, SetBrush/PenSource guaranteed that it is compatible */
/* with the current color space. */
int
px_set_paint(const px_paint_t *ppt, px_state_t *pxs)
{	gs_state *pgs = pxs->pgs;

	switch ( ppt->type )
	  {
	  case pxpNull:
	    gs_setnullcolor(pgs);
	    return 0;
	  case pxpGray:
	    return gs_setgray(pgs, ppt->value.gray);
	  case pxpRGB:
	    return gs_setrgbcolor(pgs, ppt->value.rgb[0], ppt->value.rgb[1],
				  ppt->value.rgb[2]);
	  case pxpPattern:
	    return gs_setpattern(pgs, &ppt->value.pattern.color);
	  default:		/* can't happen */
	    return_error(errorIllegalAttributeValue);
	  }
}

/* ---------------- Operators ---------------- */

const byte apxSetBrushSource[] = {
  0, pxaRGBColor, pxaGrayLevel, pxaNullBrush, pxaPatternSelectID,
  pxaPatternOrigin, pxaNewDestinationSize, 0
};
int
pxSetBrushSource(px_args_t *par, px_state_t *pxs)
{	return set_source(par, pxs, &pxs->pxgs->brush);
}

const byte apxSetColorSpace[] = {
  pxaColorSpace, 0, pxaPaletteDepth, pxaPaletteData, 0
};
int
pxSetColorSpace(px_args_t *par, px_state_t *pxs)
{	px_gstate_t *pxgs = pxs->pxgs;
	pxeColorSpace_t cspace = par->pv[0]->value.i;

	if ( par->pv[1] && par->pv[2] )
	  { int ncomp = (cspace == eRGB ? 3 : 1);
	    uint size = par->pv[2]->value.array.size;

	    if ( !(size == ncomp << 1 || size == ncomp << 4 ||
		   size == ncomp << 8)
	       )
	      return_error(errorIllegalAttributeValue);
	    /* The palette is in an array, but we want a string. */
	    { if ( pxgs->palette.data && !pxgs->palette_is_shared &&
		   pxgs->palette.size != size
		 )
	        { gs_free_string(pxs->memory, pxgs->palette.data,
				 pxgs->palette.size,
				 "pxSetColorSpace(old palette)");
		  pxgs->palette.data = 0;
		  pxgs->palette.size = 0;
		}
	      if ( pxgs->palette.data == 0 || pxgs->palette_is_shared )
		{ byte *pdata =
		    gs_alloc_string(pxs->memory, size,
				    "pxSetColorSpace(palette)");

		  if ( pdata == 0 )
		    return_error(errorInsufficientMemory);
		  pxgs->palette.data = pdata;
		  pxgs->palette.size = size;
		}
	      memcpy(pxgs->palette.data, par->pv[2]->value.array.data, size);
	    }
	  }
	else if ( par->pv[1] || par->pv[2] )
	  return_error(errorMissingAttribute);
	else if ( pxgs->palette.data )
	  { if ( !pxgs->palette_is_shared )
	      gs_free_string(pxs->memory, pxgs->palette.data,
			     pxgs->palette.size,
			     "pxSetColorSpace(old palette)");
	    pxgs->palette.data = 0;
	    pxgs->palette.size = 0;
	  }
	pxgs->palette_is_shared = false;
	pxgs->color_space = cspace;
#ifndef SET_COLOR_SPACE_NO_SET_BLACK
	  { px_paint_rc_adjust(&pxgs->brush, -1, pxs->memory);
	    pxgs->brush.type = pxpGray;
	    pxgs->brush.value.gray = 0;
	  }
	  { px_paint_rc_adjust(&pxgs->pen, -1, pxs->memory);
	    pxgs->pen.type = pxpGray;
	    pxgs->pen.value.gray = 0;
	  }
#endif
	return 0;
}

const byte apxSetHalftoneMethod[] = {
  0, pxaDitherOrigin, pxaDeviceMatrix, pxaDitherMatrixDataType,
  pxaDitherMatrixSize, pxaDitherMatrixDepth, 0
};
int
pxSetHalftoneMethod(px_args_t *par, px_state_t *pxs)
{	gs_state *pgs = pxs->pgs;
	px_gstate_t *pxgs = pxs->pxgs;
	pxeDitherMatrix_t method;

	if ( par->pv[1] )
	  { /* Internal halftone */
	    if ( par->pv[2] || par->pv[3] || par->pv[4] )
	      return_error(errorIllegalAttributeCombination);
	    method = par->pv[1]->value.i;
	    px_set_default_screen_size(pxs, method);
	    pxs->download_string.data = 0;
	    pxs->download_string.size = 0;
	  }
	else if ( par->pv[2] && par->pv[3] && par->pv[4] )
	  { /* Dither matrix */
	    uint width = par->pv[3]->value.ia[0];
	    uint source_width = (width + 3) & ~3;
	    uint height = par->pv[3]->value.ia[1];
	    uint size = width * height;
	    uint source_size = source_width * height;

	    if ( par->source.position == 0 )
	      { byte *data;
	        if ( par->source.available == 0 )
		  return pxNeedData;
		data = gs_alloc_string(pxs->memory, size, "dither matrix");
		if ( data == 0 )
		  return_error(errorInsufficientMemory);
		pxs->download_string.data = data;
		pxs->download_string.size = size;
	      }
	    while ( par->source.position < source_size )
	      { uint source_x = par->source.position % source_width;
		uint source_y = par->source.position / source_width;
		uint used;

		if ( par->source.available == 0 )
		  return pxNeedData;
		if ( source_x >= width )
		  { /* Skip padding bytes at end of row. */
		    used = min(par->source.available, source_width - source_x);
		  }
		else
		  { /* Read data. */
		    const byte *src = par->source.data;
		    byte *dest = pxs->download_string.data;
		    uint i;
		    int skip;

		    used = min(par->source.available, width - source_x);
		    /*
		     * The documentation doesn't say this, but we have to
		     * rotate the dither matrix to match the orientation,
		     * remembering that we have a Y-inverted coordinate
		     * system.  This is quite a nuisance!
		     */
		    switch ( pxs->orientation )
		      {
		      case ePortraitOrientation:
			dest += source_y * width + source_x;
			skip = 1;
			break;
		      case eLandscapeOrientation:
			dest += (width - 1 - source_x) * height + source_y;
			skip = -height;
			break;
		      case eReversePortrait:
			dest += (height - 1 - source_y) * width +
			  width - 1 - source_x;
			skip = -1;
			break;
		      case eReverseLandscape:
			dest += source_x * height + width - 1 - source_y;
			skip = height;
			break;
		      }
		    for ( i = 0; i < used; ++i, ++src, dest += skip )
		      *dest = *src;
		  }
		par->source.position += used;
		par->source.available -= used;
		par->source.data += used;

	      }
	    pxgs->halftone.width = width;
	    pxgs->halftone.height = height;
	    method = eDownloaded;
	  }
	else
	  return_error(errorMissingAttribute);
	if ( par->pv[0] )
	  gs_transform(pgs, real_value(par->pv[0], 0),
		       real_value(par->pv[0], 1), &pxgs->halftone.origin);
	else
	  gs_transform(pgs, 0.0, 0.0, &pxgs->halftone.origin);
	pxgs->halftone.thresholds = pxs->download_string;
	pxgs->halftone.method = method;
	pxgs->halftone.set = false;
	return 0;
}

const byte apxSetPenSource[] = {
  0, pxaRGBColor, pxaGrayLevel, pxaNullPen, pxaPatternSelectID,
  pxaPatternOrigin, pxaNewDestinationSize, 0
};
int
pxSetPenSource(px_args_t *par, px_state_t *pxs)
{	return set_source(par, pxs, &pxs->pxgs->pen);
}
