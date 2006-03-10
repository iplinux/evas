#include "evas_common.h"
#include "evas_private.h"
#include "evas_engine.h"
#include "Evas_Engine_XRender_Xcb.h"

static Evas_Hash *_xr_fg_pool = NULL;

XR_Font_Surface *
_xre_font_surface_new(XCBimage_Info *xcbinf, RGBA_Font_Glyph *fg)
{    
   char buf[256];
   char buf2[256];
   XR_Font_Surface *fs;
   DATA8 *data;
   int w, h, j;
   XCBimage_Image  *xcim;
   Evas_Hash *pool;
   CARD32 mask;
   CARD32 values[3];
   
   data = fg->glyph_out->bitmap.buffer;
   w = fg->glyph_out->bitmap.width;
   h = fg->glyph_out->bitmap.rows;
   j = fg->glyph_out->bitmap.pitch;
   if (j < w) j = w;
   if ((w <= 0) || (h <= 0)) return NULL;

   if (fg->ext_dat)
     {
	fs = fg->ext_dat;
	if ((fs->xcbinf->conn == xcbinf->conn) &&
            (fs->xcbinf->root.window.xid == xcbinf->root.window.xid))
	  return fs;
	snprintf(buf, sizeof(buf), "@%p@/@%lx@", fs->xcbinf->conn, fs->xcbinf->root.window.xid);
	pool = evas_hash_find(_xr_fg_pool, buf);
	if (pool)
	  {
	     snprintf(buf, sizeof(buf), "%p", fg);
	     fs = evas_hash_find(pool, buf);
	     if (fs) return fs;
	  }
     }

   fs = calloc(1, sizeof(XR_Font_Surface));
   if (!fs) return NULL;

   fs->xcbinf = xcbinf;
   fs->fg = fg;
   fs->xcbinf->references++;
   fs->w = w;
   fs->h = h;
   
   snprintf(buf, sizeof(buf), "@%p@/@%lx@", fs->xcbinf->conn, fs->xcbinf->root.window.xid);
   pool = evas_hash_find(_xr_fg_pool, buf);
   snprintf(buf2, sizeof(buf2), "%p", fg);
   pool = evas_hash_add(pool, buf2, fs);
   _xr_fg_pool = evas_hash_add(_xr_fg_pool, buf, pool);

   fs->draw.pixmap = XCBPIXMAPNew(xcbinf->conn);
   XCBCreatePixmap(xcbinf->conn, xcbinf->fmt8->depth, fs->draw.pixmap, xcbinf->root, w, h);

   mask = XCBRenderCPRepeat | XCBRenderCPDither | XCBRenderCPComponentAlpha;
   values[0] = 0;
   values[1] = 0;
   values[2] = 0;
   fs->pic = XCBRenderPICTURENew(xcbinf->conn);
   XCBRenderCreatePicture(xcbinf->conn, fs->pic, fs->draw, xcbinf->fmt8->id, mask, values);
   
   xcim = _xr_image_new(fs->xcbinf, w, h, xcbinf->fmt8->depth);
   if ((fg->glyph_out->bitmap.num_grays == 256) &&
       (fg->glyph_out->bitmap.pixel_mode == ft_pixel_mode_grays))
     {
	int x, y;
	DATA8 *p1, *p2;
	
	for (y = 0; y < h; y++)
	  {
	     p1 = data + (j * y);
	     p2 = ((DATA8 *)xcim->data) + (xcim->line_bytes * y);
	     for (x = 0; x < w; x++)
	       {
		  *p2 = *p1;
		  p1++;
		  p2++;
	       }
	  }
	
     }
   else
     {
        DATA8 *tmpbuf = NULL, *dp, *tp, bits;
	int bi, bj, end;
	const DATA8 bitrepl[2] = {0x0, 0xff};
	
	tmpbuf = alloca(w);
	  {
	     int x, y;
	     DATA8 *p1, *p2;
	     
	     for (y = 0; y < h; y++)
	       {
		  p1 = tmpbuf;
		  p2 = ((DATA8 *)xcim->data) + (xcim->line_bytes * y);
		  tp = tmpbuf;
		  dp = data + (y * fg->glyph_out->bitmap.pitch);
		  for (bi = 0; bi < w; bi += 8)
		    {
		       bits = *dp;
		       if ((w - bi) < 8) end = w - bi;
		       else end = 8;
		       for (bj = 0; bj < end; bj++)
			 {
			    *tp = bitrepl[(bits >> (7 - bj)) & 0x1];
			    tp++;
			 }
		       dp++;
		    }
		  for (x = 0; x < w; x++)
		    {
		       *p2 = *p1;
		       p1++;
		       p2++;
		    }
	       }
	  }
     }
   _xr_image_put(xcim, fs->draw, 0, 0, w, h);
   return fs;
}

static Evas_Bool
_xre_font_pool_cb(Evas_Hash *hash, const char *key, void *data, void *fdata)
{
   Evas_Hash *pool;
   XR_Font_Surface *fs;
   char buf[256];
   
   fs = fdata;
   pool = data;
   snprintf(buf, sizeof(buf), "@%p@/@%lx@", fs->xcbinf->conn, fs->xcbinf->root.window.xid);
   pool = evas_hash_del(pool, buf, fs);
   hash = evas_hash_modify(hash, key, pool);
   return 1;
}

void
_xre_font_surface_free(XR_Font_Surface *fs)
{
   if (!fs) return;
   evas_hash_foreach(_xr_fg_pool, _xre_font_pool_cb, fs);
   XCBFreePixmap(fs->xcbinf->conn, fs->draw.pixmap);
   XCBRenderFreePicture(fs->xcbinf->conn, fs->pic);
   _xr_image_info_free(fs->xcbinf);
   free(fs);
}

void
_xre_font_surface_draw(XCBimage_Info *xcbinf, RGBA_Image *surface, RGBA_Draw_Context *dc, RGBA_Font_Glyph *fg, int x, int y)
{
   XR_Font_Surface   *fs;
   XCBrender_Surface *target_surface;
   XCBRECTANGLE       rect;
   int                r;
   int                g;
   int                b;
   int                a;
   
   fs = fg->ext_dat;
   if (!fs) return;
   target_surface = (XCBrender_Surface *)(surface->image->data);
   a = (dc->col.col >> 24) & 0xff;
   r = (dc->col.col >> 16) & 0xff;
   g = (dc->col.col >> 8 ) & 0xff;
   b = (dc->col.col      ) & 0xff;
   if ((fs->xcbinf->mul_r != r) || (fs->xcbinf->mul_g != g) ||
       (fs->xcbinf->mul_b != b) || (fs->xcbinf->mul_a != a))
     {
	fs->xcbinf->mul_r = r;
	fs->xcbinf->mul_g = g;
	fs->xcbinf->mul_b = b;
	fs->xcbinf->mul_a = a;
	_xr_render_surface_solid_rectangle_set(fs->xcbinf->mul, r, g, b, a, 0, 0, 1, 1);
     }
   rect.x = x;
   rect.y = y;
   rect.width = fs->w;
   rect.height = fs->h;
   if ((dc) && (dc->clip.use))
     {
	RECTS_CLIP_TO_RECT(rect.x, rect.y, rect.width, rect.height,
			   dc->clip.x, dc->clip.y, dc->clip.w, dc->clip.h);
     }
   XCBRenderSetPictureClipRectangles(target_surface->xcbinf->conn, 
                                     target_surface->pic, 0, 0, 1, &rect);
   XCBRenderComposite(fs->xcbinf->conn, XCBRenderPictOpOver,
                      fs->xcbinf->mul->pic, 
                      fs->pic,
                      target_surface->pic,
                      0, 0,
                      0, 0,
                      x, y,
                      fs->w, fs->h);
}