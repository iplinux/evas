/*
 * vim:ts=8:sw=3:sts=8:noexpandtab:cino=>5n-3f0^-2{2
 */

#include "evas_common.h"
#include "evas_private.h"

/* save typing */
#define ENFN obj->layer->evas->engine.func
#define ENDT obj->layer->evas->engine.data.output

/* private magic number for textblock objects */
static const char o_type[] = "textblock";

/* private struct for textblock object internal data */
typedef struct _Evas_Object_Textblock             Evas_Object_Textblock;
typedef struct _Evas_Object_Style_Tag             Evas_Object_Style_Tag;
typedef struct _Evas_Object_Textblock_Node        Evas_Object_Textblock_Node;
typedef struct _Evas_Object_Textblock_Line        Evas_Object_Textblock_Line;
typedef struct _Evas_Object_Textblock_Item        Evas_Object_Textblock_Item;
typedef struct _Evas_Object_Textblock_Format_Item Evas_Object_Textblock_Format_Item;
typedef struct _Evas_Object_Textblock_Format      Evas_Object_Textblock_Format;

/* the current state of the formatting */

#define  NODE_TEXT   0
#define  NODE_FORMAT 1

struct _Evas_Object_Style_Tag
{
   EINA_INLIST;
   char *tag;
   char *replace;
   size_t tag_len;
   size_t replace_len;
};

struct _Evas_Object_Textblock_Node
{
   EINA_INLIST;
   Eina_Strbuf *text;
   int          type;
};

struct _Evas_Object_Textblock_Line
{
   EINA_INLIST;
   Evas_Object_Textblock_Item        *items;
   Evas_Object_Textblock_Format_Item *format_items;
   int                                x, y, w, h;
   int                                baseline;
   int                                line_no;
};

struct _Evas_Object_Textblock_Item
{
   EINA_INLIST;
   char                         *text;
   Evas_Object_Textblock_Format *format;
   Evas_Object_Textblock_Node   *source_node;
   int                           x, w, h;
   int                           inset, baseline;
   int                           source_pos;
   unsigned char                 type;
};

struct _Evas_Object_Textblock_Format_Item
{
   EINA_INLIST;
   const char                   *item;
   Evas_Object_Textblock_Node   *source_node;
   int                           x, w, h, y, ascent, descent;
   unsigned char                 vsize : 2;
   unsigned char                 size : 2;
   unsigned char                 formatme : 1;
   unsigned char                 ___padding___ : 3;
};

struct _Evas_Object_Textblock_Format
{
   int                  ref;
   double               halign;
   double               valign;
   struct {
      const char       *name;
      const char       *source;
      const char       *fallbacks;
      int               size;
      void             *font;
   } font;
   struct {
      struct {
	 unsigned char  r, g, b, a;
      } normal, underline, underline2, outline, shadow, glow, glow2, backing,
	strikethrough;
   } color;
   struct {
      int               l, r;
   } margin;
   int                  tabstops;
   int                  linesize;
   double               linerelsize;
   int                  linegap;
   double               linerelgap;
   double               linefill;
   unsigned char        style;
   unsigned char        wrap_word : 1;
   unsigned char        wrap_char : 1;
   unsigned char        underline : 1;
   unsigned char        underline2 : 1;
   unsigned char        strikethrough : 1;
   unsigned char        backing : 1;
   unsigned char        ellipsis_left : 1;
   unsigned char        ellipsis_right : 1;
   const char          *ellipsis_symbol;
};

struct _Evas_Textblock_Style
{
   char                  *style_text;
   char                  *default_tag;
   Evas_Object_Style_Tag *tags;
   Eina_List             *objects;
   unsigned char          delete_me : 1;
};

struct _Evas_Textblock_Cursor
{
   Evas_Object                *obj;
   int                         pos;
   Evas_Object_Textblock_Node *node;
   Eina_Bool                   eol : 1;
};

struct _Evas_Object_Textblock
{
   DATA32                       magic;
   Evas_Textblock_Style        *style;

   /*
    * Dedicated first cursor, immune to deletion and returned by
    * evas_object_textblock_cursor_get(). For the sake of consistency (and
    * avoiding code duplication) it is stored in cursors list as first element.
    */
   Evas_Textblock_Cursor       *cursor;

   Eina_List                   *cursors;
   Evas_Object_Textblock_Node  *nodes;
   Evas_Object_Textblock_Line  *lines;
   int                          last_w;
   struct {
      int                       l, r, t, b;
   } style_pad;
   char                        *markup_text;
   void                        *engine_data;
   const char                  *repch;
   struct {
      int                       w, h;
      unsigned char             valid : 1;
   } formatted, native;
   unsigned char                redraw : 1;
   unsigned char                changed : 1;
};

/* private methods for textblock objects */
static void evas_object_textblock_init(Evas_Object *obj);
static void *evas_object_textblock_new(void);
static void evas_object_textblock_render(Evas_Object *obj, void *output, void *context, void *surface, int x, int y);
static void evas_object_textblock_free(Evas_Object *obj);
static void evas_object_textblock_render_pre(Evas_Object *obj);
static void evas_object_textblock_render_post(Evas_Object *obj);

static unsigned int evas_object_textblock_id_get(Evas_Object *obj);
static unsigned int evas_object_textblock_visual_id_get(Evas_Object *obj);
static void *evas_object_textblock_engine_data_get(Evas_Object *obj);

static int evas_object_textblock_is_opaque(Evas_Object *obj);
static int evas_object_textblock_was_opaque(Evas_Object *obj);

static void evas_object_textblock_coords_recalc(Evas_Object *obj);

static void evas_object_textblock_scale_update(Evas_Object *obj);

static const Evas_Object_Func object_func =
{
   /* methods (compulsory) */
   evas_object_textblock_free,
     evas_object_textblock_render,
     evas_object_textblock_render_pre,
     evas_object_textblock_render_post,
     evas_object_textblock_id_get,
     evas_object_textblock_visual_id_get,
     evas_object_textblock_engine_data_get,
   /* these are optional. NULL = nothing */
     NULL,
     NULL,
     NULL,
     NULL,
     evas_object_textblock_is_opaque,
     evas_object_textblock_was_opaque,
     NULL,
     NULL,
     evas_object_textblock_coords_recalc,
     evas_object_textblock_scale_update,
     NULL,
     NULL,
     NULL
};

/* the actual api call to add a textblock */

#define TB_HEAD() \
   Evas_Object_Textblock *o; \
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ); \
   return; \
   MAGIC_CHECK_END(); \
   o = (Evas_Object_Textblock *)(obj->object_data); \
   MAGIC_CHECK(o, Evas_Object_Textblock, MAGIC_OBJ_TEXTBLOCK); \
   return; \
   MAGIC_CHECK_END();

#define TB_HEAD_RETURN(x) \
   Evas_Object_Textblock *o; \
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ); \
   return (x); \
   MAGIC_CHECK_END(); \
   o = (Evas_Object_Textblock *)(obj->object_data); \
   MAGIC_CHECK(o, Evas_Object_Textblock, MAGIC_OBJ_TEXTBLOCK); \
   return (x); \
   MAGIC_CHECK_END();


/**
 * @addtogroup Evas_Object_Textblock
 * @{
 */

/* styles */
static void
_style_clear(Evas_Textblock_Style *ts)
{
   if (ts->style_text) free(ts->style_text);
   if (ts->default_tag) free(ts->default_tag);
   while (ts->tags)
     {
	Evas_Object_Style_Tag *tag;

	tag = (Evas_Object_Style_Tag *)ts->tags;
	ts->tags = (Evas_Object_Style_Tag *)eina_inlist_remove(EINA_INLIST_GET(ts->tags), EINA_INLIST_GET(tag));
	free(tag->tag);
	free(tag->replace);
	free(tag);
     }
   ts->style_text = NULL;
   ts->default_tag = NULL;
   ts->tags = NULL;
}

static inline const char *
_style_match_replace(Evas_Textblock_Style *ts, const char *s, size_t replace_len, size_t *tag_len)
{
   Evas_Object_Style_Tag *tag;

   EINA_INLIST_FOREACH(ts->tags, tag)
     {
	if (tag->replace_len != replace_len) continue;
	if (!strcmp(tag->replace, s))
	  {
	     *tag_len = tag->tag_len;
	     return tag->tag;
	  }
     }
   *tag_len = 0;
   return NULL;
}

static inline const char *
_style_match_tag(Evas_Textblock_Style *ts, const char *s, size_t tag_len, size_t *replace_len)
{
   Evas_Object_Style_Tag *tag;

   EINA_INLIST_FOREACH(ts->tags, tag)
     {
	if (tag->tag_len != tag_len) continue;
	if (!strcmp(tag->tag, s))
	  {
	     *replace_len = tag->replace_len;
	     return tag->replace;
	  }
     }
   *replace_len = 0;
   return NULL;
}

static void
_nodes_clear(const Evas_Object *obj)
{
   Evas_Object_Textblock *o;

   o = (Evas_Object_Textblock *)(obj->object_data);
   while (o->nodes)
     {
	Evas_Object_Textblock_Node *n;

	n = (Evas_Object_Textblock_Node *)o->nodes;
	o->nodes = (Evas_Object_Textblock_Node *)eina_inlist_remove(EINA_INLIST_GET(o->nodes), EINA_INLIST_GET(n));
	if (n->text) eina_strbuf_free(n->text);
	free(n);
     }
}

static void
_format_free(const Evas_Object *obj, Evas_Object_Textblock_Format *fmt)
{
   fmt->ref--;
   if (fmt->ref > 0) return;
   if (fmt->font.name) eina_stringshare_del(fmt->font.name);
   if (fmt->font.fallbacks) eina_stringshare_del(fmt->font.fallbacks);
   if (fmt->font.source) eina_stringshare_del(fmt->font.source);
   if (fmt->ellipsis_symbol) eina_stringshare_del(fmt->ellipsis_symbol);
   evas_font_free(obj->layer->evas, fmt->font.font);
   free(fmt);
}

static void
_line_free(const Evas_Object *obj, Evas_Object_Textblock_Line *ln)
{
   while (ln->items)
     {
	Evas_Object_Textblock_Item *it;

	it = (Evas_Object_Textblock_Item *)ln->items;
	ln->items = (Evas_Object_Textblock_Item *)eina_inlist_remove(EINA_INLIST_GET(ln->items), EINA_INLIST_GET(ln->items));
	if (it->text) free(it->text);
	_format_free(obj, it->format);
	free(it);
     }
   while (ln->format_items)
     {
	Evas_Object_Textblock_Format_Item *fi;

	fi = (Evas_Object_Textblock_Format_Item *)ln->format_items;
	ln->format_items = (Evas_Object_Textblock_Format_Item *)eina_inlist_remove(EINA_INLIST_GET(ln->format_items),
										   EINA_INLIST_GET(ln->format_items));
	if (fi->item) eina_stringshare_del(fi->item);
	free(fi);
     }
   if (ln) free(ln);
}

static void
_lines_clear(const Evas_Object *obj, Evas_Object_Textblock_Line *lines)
{
   while (lines)
     {
	Evas_Object_Textblock_Line *ln;

	ln = (Evas_Object_Textblock_Line *)lines;
	lines = (Evas_Object_Textblock_Line *)eina_inlist_remove(EINA_INLIST_GET(lines), EINA_INLIST_GET(ln));
	_line_free(obj, ln);
     }
}

static void
_nodes_next_merge(const Evas_Object *obj, Evas_Object_Textblock_Node *cur)
{
    Evas_Object_Textblock *o;
    Evas_Object_Textblock_Node *next;
    Eina_List *l;
    Evas_Textblock_Cursor *cursor;
    int cur_len;

    if (!cur || cur->type != NODE_TEXT)
        return;

    next = (Evas_Object_Textblock_Node*)(EINA_INLIST_GET(cur))->next;

    if (!next || next->type != NODE_TEXT)
        return;

    /* Merge text */
    cur_len = eina_strbuf_length_get(cur->text);
    eina_strbuf_append_length(cur->text, eina_strbuf_string_get(next->text),
            eina_strbuf_length_get(next->text));

    /* Remove "next" from list */
    o = obj->object_data;
    o->nodes = (Evas_Object_Textblock_Node *)eina_inlist_remove
        (EINA_INLIST_GET(o->nodes), EINA_INLIST_GET(next));

    /* Fixup cursors */
    EINA_LIST_FOREACH(o->cursors, l, cursor) {
        if (next == cursor->node) {
            cursor->node = cur;
            cursor->pos += cur_len;
        }
    }

    /* Free next */
    if (next->text) eina_strbuf_free(next->text);
    free(next);
}

static void
_nodes_adjacent_merge(const Evas_Object *obj, Evas_Object_Textblock_Node *cur)
{
    Evas_Object_Textblock_Node *prev;
    prev = (Evas_Object_Textblock_Node *)EINA_INLIST_GET(cur)->prev;
    _nodes_next_merge(obj, cur);
    _nodes_next_merge(obj, prev);
}

/* table of html escapes (that i can find) this should be ordered with the
 * most common first as it's a linear search to match - no hash for this.
 *
 * these are stored as one large string and one additional array that
 * contains the offsets to the tokens for space efficiency.
 */
static const char escape_strings[] =
	/* most common escaped stuff */
	" \0"          "\x20\0" /* NOTE: this here to avoid escaping to &nbsp */
	"&nbsp;\0"     "\x20\0" /* NOTE: we allow nsbp's to break as we map early - maybe map to ascii 0x01 and then make the rendering code think 0x01 -> 0x20 */
	"&quot;\0"     "\x22\0"
	"&amp;\0"      "\x26\0"
	"&lt;\0"       "\x3c\0"
	"&gt;\0"       "\x3e\0"
	/* all the rest */
	"&iexcl;\0"    "\xc2\xa1\0"
	"&cent;\0"     "\xc2\xa2\0"
	"&pound;\0"    "\xc2\xa3\0"
	"&curren;\0"   "\xc2\xa4\0"
	"&yen;\0"      "\xc2\xa5\0"
	"&brvbar;\0"   "\xc2\xa6\0"
	"&sect;\0"     "\xc2\xa7\0"
	"&uml;\0"      "\xc2\xa8\0"
	"&copy;\0"     "\xc2\xa9\0"
	"&ordf;\0"     "\xc2\xaa\0"
	"&laquo;\0"    "\xc2\xab\0"
	"&not;\0"      "\xc2\xac\0"
	"&reg;\0"      "\xc2\xae\0"
	"&macr;\0"     "\xc2\xaf\0"
	"&deg;\0"      "\xc2\xb0\0"
	"&plusmn;\0"   "\xc2\xb1\0"
	"&sup2;\0"     "\xc2\xb2\0"
	"&sup3;\0"     "\xc2\xb3\0"
	"&acute;\0"    "\xc2\xb4\0"
	"&micro;\0"    "\xc2\xb5\0"
	"&para;\0"     "\xc2\xb6\0"
	"&middot;\0"   "\xc2\xb7\0"
	"&cedil;\0"    "\xc2\xb8\0"
	"&sup1;\0"     "\xc2\xb9\0"
	"&ordm;\0"     "\xc2\xba\0"
	"&raquo;\0"    "\xc2\xbb\0"
	"&frac14;\0"   "\xc2\xbc\0"
	"&frac12;\0"   "\xc2\xbd\0"
	"&frac34;\0"   "\xc2\xbe\0"
	"&iquest;\0"   "\xc2\xbf\0"
	"&Agrave;\0"   "\xc3\x80\0"
	"&Aacute;\0"   "\xc3\x81\0"
	"&Acirc;\0"    "\xc3\x82\0"
	"&Atilde;\0"   "\xc3\x83\0"
	"&Auml;\0"     "\xc3\x84\0"
	"&Aring;\0"    "\xc3\x85\0"
	"&Aelig;\0"    "\xc3\x86\0"
	"&Ccedil;\0"   "\xc3\x87\0"
	"&Egrave;\0"   "\xc3\x88\0"
	"&Eacute;\0"   "\xc3\x89\0"
	"&Ecirc;\0"    "\xc3\x8a\0"
	"&Euml;\0"     "\xc3\x8b\0"
	"&Igrave;\0"   "\xc3\x8c\0"
	"&Iacute;\0"   "\xc3\x8d\0"
	"&Icirc;\0"    "\xc3\x8e\0"
	"&Iuml;\0"     "\xc3\x8f\0"
	"&Eth;\0"      "\xc3\x90\0"
	"&Ntilde;\0"   "\xc3\x91\0"
	"&Ograve;\0"   "\xc3\x92\0"
	"&Oacute;\0"   "\xc3\x93\0"
	"&Ocirc;\0"    "\xc3\x94\0"
	"&Otilde;\0"   "\xc3\x95\0"
	"&Ouml;\0"     "\xc3\x96\0"
	"&times;\0"    "\xc3\x97\0"
	"&Oslash;\0"   "\xc3\x98\0"
	"&Ugrave;\0"   "\xc3\x99\0"
	"&Uacute;\0"   "\xc3\x9a\0"
	"&Ucirc;\0"    "\xc3\x9b\0"
	"&Yacute;\0"   "\xc3\x9d\0"
	"&Thorn;\0"    "\xc3\x9e\0"
	"&szlig;\0"    "\xc3\x9f\0"
	"&agrave;\0"   "\xc3\xa0\0"
	"&aacute;\0"   "\xc3\xa1\0"
	"&acirc;\0"    "\xc3\xa2\0"
	"&atilde;\0"   "\xc3\xa3\0"
	"&auml;\0"     "\xc3\xa4\0"
	"&aring;\0"    "\xc3\xa5\0"
	"&aelig;\0"    "\xc3\xa6\0"
	"&ccedil;\0"   "\xc3\xa7\0"
	"&egrave;\0"   "\xc3\xa8\0"
	"&eacute;\0"   "\xc3\xa9\0"
	"&ecirc;\0"    "\xc3\xaa\0"
	"&euml;\0"     "\xc3\xab\0"
	"&igrave;\0"   "\xc3\xac\0"
	"&iacute;\0"   "\xc3\xad\0"
	"&icirc;\0"    "\xc3\xae\0"
	"&iuml;\0"     "\xc3\xaf\0"
	"&eth;\0"      "\xc3\xb0\0"
	"&ntilde;\0"   "\xc3\xb1\0"
	"&ograve;\0"   "\xc3\xb2\0"
	"&oacute;\0"   "\xc3\xb3\0"
	"&ocirc;\0"    "\xc3\xb4\0"
	"&otilde;\0"   "\xc3\xb5\0"
	"&ouml;\0"     "\xc3\xb6\0"
	"&divide;\0"   "\xc3\xb7\0"
	"&oslash;\0"   "\xc3\xb8\0"
	"&ugrave;\0"   "\xc3\xb9\0"
	"&uacute;\0"   "\xc3\xba\0"
	"&ucirc;\0"    "\xc3\xbb\0"
	"&uuml;\0"     "\xc3\xbc\0"
	"&yacute;\0"   "\xc3\xbd\0"
	"&thorn;\0"    "\xc3\xbe\0"
	"&yuml;\0"     "\xc3\xbf\0"
	"&alpha;\0"    "\xce\x91\0"
	"&beta;\0"     "\xce\x92\0"
	"&gamma;\0"    "\xce\x93\0"
	"&delta;\0"    "\xce\x94\0"
	"&epsilon;\0"  "\xce\x95\0"
	"&zeta;\0"     "\xce\x96\0"
	"&eta;\0"      "\xce\x97\0"
	"&theta;\0"    "\xce\x98\0"
	"&iota;\0"     "\xce\x99\0"
	"&kappa;\0"    "\xce\x9a\0"
	"&lambda;\0"   "\xce\x9b\0"
	"&mu;\0"       "\xce\x9c\0"
	"&nu;\0"       "\xce\x9d\0"
	"&xi;\0"       "\xce\x9e\0"
	"&omicron;\0"  "\xce\x9f\0"
	"&pi;\0"       "\xce\xa0\0"
	"&rho;\0"      "\xce\xa1\0"
	"&sigma;\0"    "\xce\xa3\0"
	"&tau;\0"      "\xce\xa4\0"
	"&upsilon;\0"  "\xce\xa5\0"
	"&phi;\0"      "\xce\xa6\0"
	"&chi;\0"      "\xce\xa7\0"
	"&psi;\0"      "\xce\xa8\0"
	"&omega;\0"    "\xce\xa9\0"
	"&hellip;\0"   "\xe2\x80\xa6\0"
	"&euro;\0"     "\xe2\x82\xac\0"
	"&larr;\0"     "\xe2\x86\x90\0"
	"&uarr;\0"     "\xe2\x86\x91\0"
	"&rarr;\0"     "\xe2\x86\x92\0"
	"&darr;\0"     "\xe2\x86\x93\0"
	"&harr;\0"     "\xe2\x86\x94\0"
	"&larr;\0"     "\xe2\x87\x90\0"
	"&rarr;\0"     "\xe2\x87\x92\0"
	"&forall;\0"   "\xe2\x88\x80\0"
	"&exist;\0"    "\xe2\x88\x83\0"
	"&nabla;\0"    "\xe2\x88\x87\0"
	"&prod;\0"     "\xe2\x88\x8f\0"
	"&sum;\0"      "\xe2\x88\x91\0"
	"&and;\0"      "\xe2\x88\xa7\0"
	"&or;\0"       "\xe2\x88\xa8\0"
	"&int;\0"      "\xe2\x88\xab\0"
	"&ne;\0"       "\xe2\x89\xa0\0"
	"&equiv;\0"    "\xe2\x89\xa1\0"
	"&oplus;\0"    "\xe2\x8a\x95\0"
	"&perp;\0"     "\xe2\x8a\xa5\0"
	"&dagger;\0"   "\xe2\x80\xa0\0"
	"&Dagger;\0"   "\xe2\x80\xa1\0"
	"&bull;\0"     "\xe2\x80\xa2\0"
;


static int
_is_white(int c)
{
   /*
    * unicode list of whitespace chars
    *
    * 0009..000D <control-0009>..<control-000D>
    * 0020 SPACE
    * 0085 <control-0085>
    * 00A0 NO-BREAK SPACE
    * 1680 OGHAM SPACE MARK
    * 180E MONGOLIAN VOWEL SEPARATOR
    * 2000..200A EN QUAD..HAIR SPACE
    * 2028 LINE SEPARATOR
    * 2029 PARAGRAPH SEPARATOR
    * 202F NARROW NO-BREAK SPACE
    * 205F MEDIUM MATHEMATICAL SPACE
    * 3000 IDEOGRAPHIC SPACE
    */
   if (
       (c == 0x20) ||
       ((c >= 0x9) && (c <= 0xd)) ||
       (c == 0x85) ||
       (c == 0xa0) ||
       (c == 0x1680) ||
       (c == 0x180e) ||
       ((c >= 0x2000) && (c <= 0x200a)) ||
       (c == 0x2028) ||
       (c == 0x2029) ||
       (c == 0x202f) ||
       (c == 0x205f) ||
       (c == 0x3000)
       )
     return 1;
   return 0;
}

static char *
_clean_white(int clean_start, int clean_end, char *str)
{
   char *p, *p2, *str2 = NULL;
   int white, pwhite, start, ok;

   return str;
   str2 = malloc(strlen(str) + 2);
   p = str;
   p2 = str2;
   white = 0;
   start = 1;
   ok = 1;
   while (*p != 0)
     {
	pwhite = white;
	if (isspace(*p) || _is_white(*p)) white = 1;
	else white = 0;
	if ((pwhite) && (white)) ok = 0;
	else
	  {
	     if (!clean_start)
	       {
		  if ((start) && (pwhite) && (!white))
		    {
//		       *p2 = ' ';
//		       p2++;
		    }
	       }
	     ok = 1;
	     if (!white) start = 0;
	  }
	if (clean_start)
	  {
	     if ((start) && (ok)) ok = 0;
	  }
	if (ok)
	  {
	     *p2 = *p;
	     p2++;
	  }
	p++;
     }
   *p2 = 0;
   if (clean_end)
     {
	while (p2 > str2)
	  {
	     p2--;
	     if (!(isspace(*p2) || _is_white(*p2))) break;
	     *p2 = 0;
	  }
     }
   free(str);
   return str2;
}

static void
_append_text_run(Evas_Object_Textblock *o, char *s, char *p)
{
   if ((s) && (p > s))
     {
	char *ts;

	ts = alloca(p - s + 1);
	strncpy(ts, s, p - s);
	ts[p - s] = 0;
	ts = _clean_white(0, 0, ts);
	evas_textblock_cursor_text_append(o->cursor, ts);
     }
}

static void
_prepend_text_run(Evas_Object_Textblock *o, char *s, char *p)
{
   if ((s) && (p > s))
     {
	char *ts;

	ts = alloca(p - s + 1);
	strncpy(ts, s, p - s);
	ts[p - s] = 0;
	ts = _clean_white(0, 0, ts);
	evas_textblock_cursor_text_prepend(o->cursor, ts);
     }
}

/* {{{ Format parsing / handling */

static int
_hex_string_get(char ch)
{
   if ((ch >= '0') && (ch <= '9')) return (ch - '0');
   else if ((ch >= 'A') && (ch <= 'F')) return (ch - 'A' + 10);
   else if ((ch >= 'a') && (ch <= 'f')) return (ch - 'a' + 10);
   return 0;
}

static void
_format_color_parse(const char *str, unsigned char *r, unsigned char *g, unsigned char *b, unsigned char *a)
{
   int slen;

   slen = strlen(str);
   *r = *g = *b = *a = 0;

   if (slen == 7) /* #RRGGBB */
     {
	*r = (_hex_string_get(str[1]) << 4) | (_hex_string_get(str[2]));
	*g = (_hex_string_get(str[3]) << 4) | (_hex_string_get(str[4]));
	*b = (_hex_string_get(str[5]) << 4) | (_hex_string_get(str[6]));
	*a = 0xff;
     }
   else if (slen == 9) /* #RRGGBBAA */
     {
	*r = (_hex_string_get(str[1]) << 4) | (_hex_string_get(str[2]));
	*g = (_hex_string_get(str[3]) << 4) | (_hex_string_get(str[4]));
	*b = (_hex_string_get(str[5]) << 4) | (_hex_string_get(str[6]));
	*a = (_hex_string_get(str[7]) << 4) | (_hex_string_get(str[8]));
     }
   else if (slen == 4) /* #RGB */
     {
	*r = _hex_string_get(str[1]);
	*r = (*r << 4) | *r;
	*g = _hex_string_get(str[2]);
	*g = (*g << 4) | *g;
	*b = _hex_string_get(str[3]);
	*b = (*b << 4) | *b;
	*a = 0xff;
     }
   else if (slen == 5) /* #RGBA */
     {
	*r = _hex_string_get(str[1]);
	*r = (*r << 4) | *r;
	*g = _hex_string_get(str[2]);
	*g = (*g << 4) | *g;
	*b = _hex_string_get(str[3]);
	*b = (*b << 4) | *b;
	*a = _hex_string_get(str[4]);
	*a = (*a << 4) | *a;
     }
   *r = (*r * *a) / 255;
   *g = (*g * *a) / 255;
   *b = (*b * *a) / 255;
}

static const char *fontstr = NULL;
static const char *font_fallbacksstr = NULL;
static const char *font_sizestr = NULL;
static const char *font_sourcestr = NULL;
static const char *colorstr = NULL;
static const char *underline_colorstr = NULL;
static const char *underline2_colorstr = NULL;
static const char *outline_colorstr = NULL;
static const char *shadow_colorstr = NULL;
static const char *glow_colorstr = NULL;
static const char *glow2_colorstr = NULL;
static const char *backing_colorstr = NULL;
static const char *strikethrough_colorstr = NULL;
static const char *alignstr = NULL;
static const char *valignstr = NULL;
static const char *wrapstr = NULL;
static const char *left_marginstr = NULL;
static const char *right_marginstr = NULL;
static const char *underlinestr = NULL;
static const char *strikethroughstr = NULL;
static const char *ellipsisstr = NULL;
static const char *ellipsis_symbolstr = NULL;
static const char *backingstr = NULL;
static const char *stylestr = NULL;
static const char *tabstopsstr = NULL;
static const char *linesizestr = NULL;
static const char *linerelsizestr = NULL;
static const char *linegapstr = NULL;
static const char *linerelgapstr = NULL;
static const char *itemstr = NULL;
static const char *linefillstr = NULL;

static void
_format_command_init(void)
{
   if (fontstr) return;
   fontstr = eina_stringshare_add("font");
   font_fallbacksstr = eina_stringshare_add("font_fallbacks");
   font_sizestr = eina_stringshare_add("font_size");
   font_sourcestr = eina_stringshare_add("font_source");
   colorstr = eina_stringshare_add("color");
   underline_colorstr = eina_stringshare_add("underline_color");
   underline2_colorstr = eina_stringshare_add("underline2_color");
   outline_colorstr = eina_stringshare_add("outline_color");
   shadow_colorstr = eina_stringshare_add("shadow_color");
   glow_colorstr = eina_stringshare_add("glow_color");
   glow2_colorstr = eina_stringshare_add("glow2_color");
   backing_colorstr = eina_stringshare_add("backing_color");
   strikethrough_colorstr = eina_stringshare_add("strikethrough_color");
   alignstr = eina_stringshare_add("align");
   valignstr = eina_stringshare_add("valign");
   wrapstr = eina_stringshare_add("wrap");
   left_marginstr = eina_stringshare_add("left_margin");
   right_marginstr = eina_stringshare_add("right_margin");
   underlinestr = eina_stringshare_add("underline");
   strikethroughstr = eina_stringshare_add("strikethrough");
   ellipsisstr = eina_stringshare_add("ellipsis");
   ellipsis_symbolstr = eina_stringshare_add("ellipsis_symbol");
   backingstr = eina_stringshare_add("backing");
   stylestr = eina_stringshare_add("style");
   tabstopsstr = eina_stringshare_add("tabstops");
   linesizestr = eina_stringshare_add("linesize");
   linerelsizestr = eina_stringshare_add("linerelsize");
   linegapstr = eina_stringshare_add("linegap");
   linerelgapstr = eina_stringshare_add("linerelgap");
   itemstr = eina_stringshare_add("item");
   linefillstr = eina_stringshare_add("linefill");
}

static void
_format_command_shutdown(void)
{
   return;
   eina_stringshare_del(fontstr);
   eina_stringshare_del(font_fallbacksstr);
   eina_stringshare_del(font_sizestr);
   eina_stringshare_del(font_sourcestr);
   eina_stringshare_del(colorstr);
   eina_stringshare_del(underline_colorstr);
   eina_stringshare_del(underline2_colorstr);
   eina_stringshare_del(outline_colorstr);
   eina_stringshare_del(shadow_colorstr);
   eina_stringshare_del(glow_colorstr);
   eina_stringshare_del(glow2_colorstr);
   eina_stringshare_del(backing_colorstr);
   eina_stringshare_del(strikethrough_colorstr);
   eina_stringshare_del(alignstr);
   eina_stringshare_del(valignstr);
   eina_stringshare_del(wrapstr);
   eina_stringshare_del(left_marginstr);
   eina_stringshare_del(right_marginstr);
   eina_stringshare_del(underlinestr);
   eina_stringshare_del(strikethroughstr);
   eina_stringshare_del(ellipsisstr);
   eina_stringshare_del(ellipsis_symbolstr);
   eina_stringshare_del(backingstr);
   eina_stringshare_del(stylestr);
   eina_stringshare_del(tabstopsstr);
   eina_stringshare_del(linesizestr);
   eina_stringshare_del(linerelsizestr);
   eina_stringshare_del(linegapstr);
   eina_stringshare_del(linerelgapstr);
   eina_stringshare_del(itemstr);
   eina_stringshare_del(linefillstr);
}

static void
_format_clean_param(char *dst, const char *src)
{
   const char *ss;
   char *ds;

   ds = dst;
   for (ss = src; *ss; ss++, ds++)
     {
        if ((*ss == '\\') && *(ss + 1)) ss++;
        *ds = *ss;
     }
   *ds = 0;
}

static void
_format_command(Evas_Object *obj, Evas_Object_Textblock_Format *fmt, const char *cmd, const char *param)
{
   int new_font = 0;
   char *tmp_param;

   tmp_param = alloca(strlen(param) + 1);

   _format_clean_param(tmp_param, param);
   if (cmd == fontstr)
     {
	if ((!fmt->font.name) ||
	    ((fmt->font.name) && (strcmp(fmt->font.name, tmp_param))))
	  {
	     if (fmt->font.name) eina_stringshare_del(fmt->font.name);
	     fmt->font.name = eina_stringshare_add(tmp_param);
	     new_font = 1;
	  }
     }
   else if (cmd == font_fallbacksstr)
     {
	if ((!fmt->font.fallbacks) ||
	    ((fmt->font.fallbacks) && (strcmp(fmt->font.fallbacks, tmp_param))))
	  {
	     /* policy - when we say "fallbacks" do we prepend and use prior
	      * fallbacks... or should we replace. for now we replace
	      */
	     if (fmt->font.fallbacks) eina_stringshare_del(fmt->font.fallbacks);
	     fmt->font.fallbacks = eina_stringshare_add(tmp_param);
	     new_font = 1;
	  }
     }
   else if (cmd == font_sizestr)
     {
	int v;

	v = atoi(tmp_param);
	if (v != fmt->font.size)
	  {
	     fmt->font.size = v;
	     new_font = 1;
	  }
     }
   else if (cmd == font_sourcestr)
     {
	if ((!fmt->font.source) ||
	    ((fmt->font.source) && (strcmp(fmt->font.source, tmp_param))))
	  {
	     if (fmt->font.source) eina_stringshare_del(fmt->font.source);
	     fmt->font.source = eina_stringshare_add(tmp_param);
	     new_font = 1;
	  }
     }
   else if (cmd == colorstr)
     _format_color_parse(tmp_param,
			 &(fmt->color.normal.r), &(fmt->color.normal.g),
			 &(fmt->color.normal.b), &(fmt->color.normal.a));
   else if (cmd == underline_colorstr)
     _format_color_parse(tmp_param,
			 &(fmt->color.underline.r), &(fmt->color.underline.g),
			 &(fmt->color.underline.b), &(fmt->color.underline.a));
   else if (cmd == underline2_colorstr)
     _format_color_parse(tmp_param,
			 &(fmt->color.underline2.r), &(fmt->color.underline2.g),
			 &(fmt->color.underline2.b), &(fmt->color.underline2.a));
   else if (cmd == outline_colorstr)
     _format_color_parse(tmp_param,
			 &(fmt->color.outline.r), &(fmt->color.outline.g),
			 &(fmt->color.outline.b), &(fmt->color.outline.a));
   else if (cmd == shadow_colorstr)
     _format_color_parse(tmp_param,
			 &(fmt->color.shadow.r), &(fmt->color.shadow.g),
			 &(fmt->color.shadow.b), &(fmt->color.shadow.a));
   else if (cmd == glow_colorstr)
     _format_color_parse(tmp_param,
			 &(fmt->color.glow.r), &(fmt->color.glow.g),
			 &(fmt->color.glow.b), &(fmt->color.glow.a));
   else if (cmd == glow2_colorstr)
     _format_color_parse(tmp_param,
			 &(fmt->color.glow2.r), &(fmt->color.glow2.g),
			 &(fmt->color.glow2.b), &(fmt->color.glow2.a));
   else if (cmd == backing_colorstr)
     _format_color_parse(tmp_param,
			 &(fmt->color.backing.r), &(fmt->color.backing.g),
			 &(fmt->color.backing.b), &(fmt->color.backing.a));
   else if (cmd == strikethrough_colorstr)
     _format_color_parse(tmp_param,
			 &(fmt->color.strikethrough.r), &(fmt->color.strikethrough.g),
			 &(fmt->color.strikethrough.b), &(fmt->color.strikethrough.a));
   else if (cmd == alignstr)
     {
	if (!strcmp(tmp_param, "middle")) fmt->halign = 0.5;
	else if (!strcmp(tmp_param, "center")) fmt->halign = 0.5;
	else if (!strcmp(tmp_param, "left")) fmt->halign = 0.0;
	else if (!strcmp(tmp_param, "right")) fmt->halign = 1.0;
	else
	  {
	     char *endptr = NULL;
	     double val = strtod(tmp_param, &endptr);
	     if (endptr)
	       {
		  while (*endptr && _is_white(*endptr))
		    endptr++;
		  if (*endptr == '%')
		    val /= 100.0;
	       }
	     fmt->halign = val;;
	     if (fmt->halign < 0.0) fmt->halign = 0.0;
	     else if (fmt->halign > 1.0) fmt->halign = 1.0;
	  }
     }
   else if (cmd == valignstr)
     {
        if (!strcmp(tmp_param, "top")) fmt->valign = 0.0;
	else if (!strcmp(tmp_param, "middle")) fmt->valign = 0.5;
	else if (!strcmp(tmp_param, "center")) fmt->valign = 0.5;
	else if (!strcmp(tmp_param, "bottom")) fmt->valign = 1.0;
	else if (!strcmp(tmp_param, "baseline")) fmt->valign = -1.0;
	else if (!strcmp(tmp_param, "base")) fmt->valign = -1.0;
	else
	  {
	     char *endptr = NULL;
	     double val = strtod(tmp_param, &endptr);
	     if (endptr)
	       {
		  while (*endptr && _is_white(*endptr))
		    endptr++;
		  if (*endptr == '%')
		    val /= 100.0;
	       }
	     fmt->valign = val;
	     if (fmt->valign < 0.0) fmt->valign = 0.0;
	     else if (fmt->valign > 1.0) fmt->valign = 1.0;
	  }
     }
   else if (cmd == wrapstr)
     {
	if (!strcmp(tmp_param, "word"))
	  {
	     fmt->wrap_word = 1;
	     fmt->wrap_char = 0;
	  }
	else if (!strcmp(tmp_param, "char"))
	  {
	     fmt->wrap_word = 0;
	     fmt->wrap_char = 1;
	  }
	else
	  {
	     fmt->wrap_word = 0;
	     fmt->wrap_char = 0;
	  }
     }
   else if (cmd == left_marginstr)
     {
	if (!strcmp(tmp_param, "reset"))
	  fmt->margin.l = 0;
	else
	  {
	     if (tmp_param[0] == '+')
	       fmt->margin.l += atoi(&(tmp_param[1]));
	     else if (tmp_param[0] == '-')
	       fmt->margin.l -= atoi(&(tmp_param[1]));
	     else
	       fmt->margin.l = atoi(tmp_param);
	     if (fmt->margin.l < 0) fmt->margin.l = 0;
	  }
     }
   else if (cmd == right_marginstr)
     {
	if (!strcmp(tmp_param, "reset"))
	  fmt->margin.r = 0;
	else
	  {
	     if (tmp_param[0] == '+')
	       fmt->margin.r += atoi(&(tmp_param[1]));
	     else if (tmp_param[0] == '-')
	       fmt->margin.r -= atoi(&(tmp_param[1]));
	     else
	       fmt->margin.r = atoi(tmp_param);
	     if (fmt->margin.r < 0) fmt->margin.r = 0;
	  }
     }
   else if (cmd == underlinestr)
     {
	if (!strcmp(tmp_param, "off"))
	  {
	     fmt->underline = 0;
	     fmt->underline2 = 0;
	  }
	else if ((!strcmp(tmp_param, "on")) ||
		 (!strcmp(tmp_param, "single")))
	  {
	     fmt->underline = 1;
	     fmt->underline2 = 0;
	  }
	else if (!strcmp(tmp_param, "double"))
	  {
	     fmt->underline = 1;
	     fmt->underline2 = 1;
	  }
     }
   else if (cmd == strikethroughstr)
     {
	if (!strcmp(tmp_param, "off"))
	  fmt->strikethrough = 0;
	else if (!strcmp(tmp_param, "on"))
	  fmt->strikethrough = 1;
     }
   else if (cmd == ellipsisstr)
     {
         if (!strcmp(tmp_param, "left"))
             fmt->ellipsis_left = 1;
         else if (!strcmp(tmp_param, "right"))
             fmt->ellipsis_right = 1;
         else if (!strcmp(tmp_param, "none"))
             fmt->ellipsis_left = fmt->ellipsis_right = 0;
     }
   else if (cmd == ellipsis_symbolstr)
     {
         if (fmt->ellipsis_symbol) eina_stringshare_del(fmt->ellipsis_symbol);
         fmt->ellipsis_symbol = eina_stringshare_add(tmp_param);
     }
   else if (cmd == backingstr)
     {
	if (!strcmp(tmp_param, "off"))
	  fmt->backing = 0;
	else if (!strcmp(tmp_param, "on"))
	  fmt->backing = 1;
     }
   else if (cmd == stylestr)
     {
	if (!strcmp(tmp_param, "off")) fmt->style = EVAS_TEXT_STYLE_PLAIN;
	else if (!strcmp(tmp_param, "none")) fmt->style = EVAS_TEXT_STYLE_PLAIN;
	else if (!strcmp(tmp_param, "plain")) fmt->style = EVAS_TEXT_STYLE_PLAIN;
	else if (!strcmp(tmp_param, "shadow")) fmt->style = EVAS_TEXT_STYLE_SHADOW;
	else if (!strcmp(tmp_param, "outline")) fmt->style = EVAS_TEXT_STYLE_OUTLINE;
	else if (!strcmp(tmp_param, "soft_outline")) fmt->style = EVAS_TEXT_STYLE_SOFT_OUTLINE;
	else if (!strcmp(tmp_param, "outline_shadow")) fmt->style = EVAS_TEXT_STYLE_OUTLINE_SHADOW;
	else if (!strcmp(tmp_param, "outline_soft_shadow")) fmt->style = EVAS_TEXT_STYLE_OUTLINE_SOFT_SHADOW;
	else if (!strcmp(tmp_param, "glow")) fmt->style = EVAS_TEXT_STYLE_GLOW;
	else if (!strcmp(tmp_param, "far_shadow")) fmt->style = EVAS_TEXT_STYLE_FAR_SHADOW;
	else if (!strcmp(tmp_param, "soft_shadow")) fmt->style = EVAS_TEXT_STYLE_SOFT_SHADOW;
	else if (!strcmp(tmp_param, "far_soft_shadow")) fmt->style = EVAS_TEXT_STYLE_FAR_SOFT_SHADOW;
	else fmt->style = EVAS_TEXT_STYLE_PLAIN;
     }
   else if (cmd == tabstopsstr)
     {
	fmt->tabstops = atoi(tmp_param);
	if (fmt->tabstops < 1) fmt->tabstops = 1;
     }
   else if (cmd == linesizestr)
     {
        fmt->linesize = atoi(tmp_param);
        fmt->linerelsize = 0.0;
     }
   else if (cmd == linerelsizestr)
     {
	char *endptr = NULL;
	double val = strtod(tmp_param, &endptr);
	if (endptr)
	  {
	     while (*endptr && _is_white(*endptr))
	       endptr++;
	     if (*endptr == '%')
	       {
		  fmt->linerelsize = val / 100.0;
		  fmt->linesize = 0;
		  if (fmt->linerelsize < 0.0) fmt->linerelsize = 0.0;
	       }
          }
     }
   else if (cmd == linegapstr)
     {
        fmt->linegap = atoi(tmp_param);
        fmt->linerelgap = 0.0;
     }
   else if (cmd == linerelgapstr)
     {
	char *endptr = NULL;
	double val = strtod(tmp_param, &endptr);
	if (endptr)
	  {
	     while (*endptr && _is_white(*endptr))
	       endptr++;
	     if (*endptr == '%')
	       {
		  fmt->linerelgap = val / 100.0;
		  fmt->linegap = 0;
		  if (fmt->linerelgap < 0.0) fmt->linerelgap = 0.0;
	       }
          }
     }
   else if (cmd == itemstr)
     {
        // itemstr == replacement object items in textblock - inline imges
        // for example
     }
   else if (cmd == linefillstr)
     {
	char *endptr = NULL;
	double val = strtod(tmp_param, &endptr);
	if (endptr)
	  {
	     while (*endptr && _is_white(*endptr))
	       endptr++;
	     if (*endptr == '%')
	       {
		  fmt->linefill = val / 100.0;
		  if (fmt->linefill < 0.0) fmt->linefill = 0.0;
	       }
          }
     }

   if (new_font)
     {
	void *of;
	char *buf = NULL;

	of = fmt->font.font;
	if ((fmt->font.name) && (fmt->font.fallbacks))
	  {
	     buf = malloc(strlen(fmt->font.name) + 1 + strlen(fmt->font.fallbacks) + 1);
	     strcpy(buf, fmt->font.name);
	     strcat(buf, ",");
	     strcat(buf, fmt->font.fallbacks);
	  }
	else if (fmt->font.name)
	  buf = strdup(fmt->font.name);

	fmt->font.font = evas_font_load(obj->layer->evas,
					buf, fmt->font.source,
					(int)(((double)fmt->font.size) * obj->cur.scale));
	if (buf) free(buf);
	if (of) evas_font_free(obj->layer->evas, of);
     }
}

static int
_format_is_param(char *item)
{
   if (strchr(item, '=')) return 1;
   return 0;
}

static void
_format_param_parse(char *item, const char **key, const char **val)
{
   char *p;
   const char *k, *v;

   p = strchr(item, '=');
   *p = '\0';
   k = eina_stringshare_add(item);
   *key = k;
   *p = '=';
   p++;
   v = eina_stringshare_add(p);
   *val = v;
}

static char *
_format_parse(char **s)
{
   char *p, *item;
   char *s1 = NULL, *s2 = NULL;

   p = *s;
   if (*p == 0) return NULL;
   for (;;)
     {
	if (!s1)
	  {
	     if (*p != ' ') s1 = p;
	     if (*p == 0) break;
	  }
	else if (!s2)
	  {
	     if ((p > *s) && (p[-1] != '\\'))
	       {
		  if (*p == ' ') s2 = p;
	       }
	     if (*p == 0) s2 = p;
	  }
	p++;
	if (s1 && s2)
	  {
	     item = s1;

	     *s = s2;
	     return item;
	  }
     }
   *s = p;
   return NULL;
}

static void
_format_fill(Evas_Object *obj, Evas_Object_Textblock_Format *fmt, char *str)
{
   char *s;
   char *item;

   s = str;

   /* get rid of anything +s or -s off the start of the string */
   while ((*s == ' ') || (*s == '+') || (*s == '-')) s++;

   while ((item = _format_parse(&s)))
     {
	char tmp_delim = *s;
	*s = '\0';
	if (_format_is_param(item))
	  {
	     const char *key = NULL, *val = NULL;

	     _format_param_parse(item, &key, &val);
	     _format_command(obj, fmt, key, val);
	     eina_stringshare_del(key);
	     eina_stringshare_del(val);
	  }
	else
	  {
	     /* immediate - not handled here */
	  }
	*s = tmp_delim;
     }
}

static Evas_Object_Textblock_Format *
_format_dup(Evas_Object *obj, Evas_Object_Textblock_Format *fmt)
{
   Evas_Object_Textblock_Format *fmt2;
   char *buf = NULL;

   fmt2 = calloc(1, sizeof(Evas_Object_Textblock_Format));
   memcpy(fmt2, fmt, sizeof(Evas_Object_Textblock_Format));
   fmt2->ref = 1;
   if (fmt->font.name) fmt2->font.name = eina_stringshare_add(fmt->font.name);
   if (fmt->font.fallbacks) fmt2->font.fallbacks = eina_stringshare_add(fmt->font.fallbacks);
   if (fmt->font.source) fmt2->font.source = eina_stringshare_add(fmt->font.source);
   if (fmt->ellipsis_symbol) fmt2->ellipsis_symbol = eina_stringshare_add(fmt->ellipsis_symbol);

   if ((fmt2->font.name) && (fmt2->font.fallbacks))
     {
	buf = malloc(strlen(fmt2->font.name) + 1 + strlen(fmt2->font.fallbacks) + 1);
	strcpy(buf, fmt2->font.name);
	strcat(buf, ",");
	strcat(buf, fmt2->font.fallbacks);
     }
   else if (fmt2->font.name)
     buf = strdup(fmt2->font.name);
   fmt2->font.font = evas_font_load(obj->layer->evas,
				    buf, fmt2->font.source,
				    (int)(((double)fmt2->font.size) * obj->cur.scale));
   if (buf) free(buf);
   return fmt2;
}

/* }}} */








typedef struct _Ctxt Ctxt;

struct _Ctxt
{
   Evas_Object *obj;
   Evas_Object_Textblock *o;

   Evas_Object_Textblock_Line *lines;
   Evas_Object_Textblock_Line *ln;

   Eina_List *format_stack;

   int x, y;
   int w, h;
   int wmax, hmax;
   int marginl, marginr;
   int line_no;
   int underline_extend;
   int have_underline, have_underline2;
   double align;
};

static void
_layout_format_ascent_descent_adjust(Ctxt *c, Evas_Object_Textblock_Format *fmt,
                                     int *maxascent, int *maxdescent)
{
   int ascent, descent;

   if (fmt->font.font)
     {
        ascent = c->ENFN->font_ascent_get(c->ENDT, fmt->font.font);
        descent = c->ENFN->font_descent_get(c->ENDT, fmt->font.font);

        if (fmt->linesize > 0)
          {
             ascent = ((fmt->linesize * ascent) / (ascent + descent));
             descent = fmt->linesize - ascent;
          }
        else if (fmt->linerelsize > 0.0)
          {
             descent = ((ascent + descent) * fmt->linerelsize) - (ascent * fmt->linerelsize);
             ascent = ascent * fmt->linerelsize;
          }
        if (*maxascent < ascent)
           *maxascent = ascent;
        if (*maxdescent < descent)
           *maxdescent = descent;
     }
}

static void
_layout_line_new(Ctxt *c, Evas_Object_Textblock_Format *fmt)
{
   c->ln = calloc(1, sizeof(Evas_Object_Textblock_Line));
   c->align = fmt->halign;
   c->marginl = fmt->margin.l;
   c->marginr = fmt->margin.r;
   c->lines = (Evas_Object_Textblock_Line *)eina_inlist_append(EINA_INLIST_GET(c->lines), EINA_INLIST_GET(c->ln));
   c->x = 0;
   c->ln->line_no = -1;
}

static Evas_Object_Textblock_Format *
_layout_format_push(Ctxt *c, Evas_Object_Textblock_Format *fmt)
{
   if (fmt)
     {
	fmt = _format_dup(c->obj, fmt);
	c->format_stack  = eina_list_prepend(c->format_stack, fmt);
     }
   else
     {
	fmt = calloc(1, sizeof(Evas_Object_Textblock_Format));
	c->format_stack  = eina_list_prepend(c->format_stack, fmt);
	fmt->ref = 1;
	fmt->halign = 0.0;
	fmt->valign = -1.0;
	fmt->style = EVAS_TEXT_STYLE_PLAIN;
	fmt->tabstops = 32;
        fmt->linesize = 0;
        fmt->linerelsize = 0.0;
        fmt->linegap = 0;
        fmt->linerelgap = 0.0;
     }
   return fmt;
}

static Evas_Object_Textblock_Format *
_layout_format_pop(Ctxt *c, Evas_Object_Textblock_Format *fmt)
{
   if ((c->format_stack) && (c->format_stack->next))
     {
	_format_free(c->obj, fmt);
	c->format_stack = eina_list_remove_list(c->format_stack, c->format_stack);
	fmt = c->format_stack->data;
     }
   return fmt;
}

static void
_layout_format_value_handle(Ctxt *c, Evas_Object_Textblock_Format *fmt, char *item)
{
   const char *key = NULL, *val = NULL;

   _format_param_parse(item, &key, &val);
   if ((key) && (val)) _format_command(c->obj, fmt, key, val);
   if (key) eina_stringshare_del(key);
   if (val) eina_stringshare_del(val);
   c->align = fmt->halign;
   c->marginl = fmt->margin.l;
   c->marginr = fmt->margin.r;
}

#define VSIZE_FULL 0
#define VSIZE_ASCENT 1

#define SIZE 0
#define SIZE_ABS 1
#define SIZE_REL 2

static void
_layout_line_finish(Ctxt* c, Evas_Object_Textblock_Format *fmt)
{
   Evas_Object_Textblock_Item *it;
   Evas_Object_Textblock_Format_Item *fi;

   /* Line to be layed out */
   Evas_Object_Textblock_Line *line = c->ln;

   /* Calculate maxascent, maxdescent for current line */
   int maxascent = 0;
   int maxdescent = 0;

   /* FIXME: make sure there is at least one item in each line? */
   if (!line->items)
      _layout_format_ascent_descent_adjust(c, fmt, &maxascent, &maxdescent);
   EINA_INLIST_FOREACH(line->items, it)
      _layout_format_ascent_descent_adjust(c, it->format, &maxascent, &maxdescent);

   /*
    * Evas_Object_Textblock_Format_Item *fi;
    * #define VSIZE_FULL 0
    * #define VSIZE_ASCENT 1
    * #define SIZE 0
    * #define SIZE_ABS 1
    * #define SIZE_REL 2
    *
   EINA_INLIST_FOREACH(c->ln->format_items, fi)
     {
	int endx;

        if (!fi->formatme) continue;
	endx = fi->x + fi->w;
	if (endx > c->ln->w) c->ln->w = endx;
        switch (fi->size)
          {
          case SIZE:
          case SIZE_ABS:
             switch (fi->vsize)
               {
               case VSIZE_FULL:
                  if (fi->h > (c->maxdescent + c->maxascent))
                    {
                       c->maxascent += fi->h - (c->maxdescent + c->maxascent);
                       fi->y = -c->maxascent;
                    }
                  else
                    fi->y = -(fi->h - c->maxdescent);
                  break;
               case VSIZE_ASCENT:
                  if (fi->h > c->maxascent)
                    {
                       c->maxascent = fi->h;
                       fi->y = -fi->h;
                    }
                  else
                    fi->y = -fi->h;
                  break;
               default:
                  break;
               }
             break;
          case SIZE_REL:
             switch (fi->vsize)
               {
               case VSIZE_FULL:
               case VSIZE_ASCENT:
                  fi->y = -fi->ascent;
                  break;
               default:
                  break;
               }
             break;
          default:
             break;
          }
     }
   */

   /* Fill in baseline for each textblock item */
   EINA_INLIST_FOREACH(line->items, it)
      if (it->format->font.font)
         it->baseline = c->ENFN->font_ascent_get(c->ENDT, it->format->font.font);

   /* Calculate width of line */
   EINA_INLIST_FOREACH(line->items, it)
      if (it->x + it->w > line->w) line->w = it->x + it->w;

   /* Adjust (?) line vertical position */
   line->y = c->y + c->o->style_pad.t;

   /* Calculate line height */
   line->h = maxascent + maxdescent;

   /* Calculate (???) line baseline */
   line->baseline = maxascent;

   /* Calculate adjustments for underline */
   if (c->have_underline2)
     {
	if (maxdescent < 4) c->underline_extend = 4 - maxdescent;
     }
   else if (c->have_underline)
     {
	if (maxdescent < 2) c->underline_extend = 2 - maxdescent;
     }

   /* Store current line number */
   line->line_no = c->line_no;
   c->line_no++;

   /* Calculate current position in context for next line */
   c->y += line->h;

   /* Calculate line horizontal position and c->wmax */

     /* If there is no width, align left */
   double align = c->w >= 0 ? c->align : 0.0;

   line->x = c->marginl + c->o->style_pad.l +
      ((c->w - line->w -
        c->o->style_pad.l - c->o->style_pad.r -
        c->marginl - c->marginr) * c->align);
   if ((line->x + line->w + c->marginr - c->o->style_pad.l) > c->wmax)
      c->wmax = line->x + line->w + c->marginl + c->marginr - c->o->style_pad.l;
}


static void
_layout_line_advance(Ctxt *c, Evas_Object_Textblock_Format *fmt)
{
   _layout_line_finish(c, fmt);
   _layout_line_new(c, fmt);
}

static Evas_Object_Textblock_Item *
_layout_item_new(Ctxt *c __UNUSED__, Evas_Object_Textblock_Format *fmt, char *str)
{
   Evas_Object_Textblock_Item *it;

   it = calloc(1, sizeof(Evas_Object_Textblock_Item));
   it->format = fmt;
   it->format->ref++;
   it->text = strdup(str);
   return it;
}

static int
_layout_text_cutoff_get(Ctxt *c, Evas_Object_Textblock_Format *fmt, Evas_Object_Textblock_Item *it)
{

   if (fmt->font.font)
     return c->ENFN->font_last_up_to_pos(c->ENDT, fmt->font.font, it->text,
					     c->w -
					     c->o->style_pad.l -
					     c->o->style_pad.r -
					     c->marginl -
					     c->marginr -
					     c->x,
					     0);
   return -1;
}

static void
_layout_item_text_cutoff(Ctxt *c __UNUSED__, Evas_Object_Textblock_Item *it, int cut)
{
   char *ts;

   ts = it->text;
   ts[cut] = 0;
   it->text = strdup(ts);
   free(ts);
}

static int
_layout_word_start(char *str, int start)
{
   int p, tp, chr = 0;

   p = start;
   chr = evas_common_font_utf8_get_next((unsigned char *)(str), &p);
   if (_is_white(chr))
     {
	tp = p;
	while (_is_white(chr) && (p >= 0))
	  {
	     tp = p;
	     chr = evas_common_font_utf8_get_next((unsigned char *)(str), &p);
	  }
	return tp;
     }
   p = start;
   tp = p;
   while (p > 0)
     {
	chr = evas_common_font_utf8_get_prev((unsigned char *)(str), &p);
	if (_is_white(chr)) break;
	tp = p;
     }
   if (p < 0) p = 0;
   if ((p >= 0) && (_is_white(chr)))
     evas_common_font_utf8_get_next((unsigned char *)(str), &p);
   return p;
}

static int
_layout_ends_with_space(char *str)
{
   int p, chr;

   p = evas_common_font_utf8_get_last((unsigned char *)(str), strlen(str));
   if (p < 0) return 0;
   chr = evas_common_font_utf8_get_next((unsigned char *)(str), &p);
   return _is_white(chr);
}

static int
_layout_strip_trailing_whitespace(Ctxt *c, Evas_Object_Textblock_Format *fmt __UNUSED__, Evas_Object_Textblock_Item *it)
{
   int p, tp, chr, adv, tw, th;

   p = evas_common_font_utf8_get_last((unsigned char *)(it->text), strlen(it->text));
   tp = p;
   if (p >= 0)
     {
	chr = evas_common_font_utf8_get_prev((unsigned char *)(it->text), &p);
	if (_is_white(chr))
	  {
	     _layout_item_text_cutoff(c, it, tp);
	     adv = 0;
	     if (it->format->font.font)
	       adv = c->ENFN->font_h_advance_get(c->ENDT, it->format->font.font, it->text);
	     tw = th = 0;
	     if (it->format->font.font)
	       c->ENFN->font_string_size_get(c->ENDT, it->format->font.font, it->text, &tw, &th);
	     it->w = tw;
	     it->h = th;
	     c->x = it->x + adv;
	     return 1;
	  }
     }
   return 0;
}

static int
_layout_item_abort(Ctxt *c, Evas_Object_Textblock_Format *fmt, Evas_Object_Textblock_Item *it)
{
   if (it->text) free(it->text);
   _format_free(c->obj, it->format);
   free(it);
   if (c->ln->items)
     {
	it = (Evas_Object_Textblock_Item *)(EINA_INLIST_GET(c->ln->items))->last;
	return _layout_strip_trailing_whitespace(c, fmt, it);
     }
   return 0;
}

#if 0
static char *
_layout_next_char_jump(Ctxt *c, Evas_Object_Textblock_Item *it, char *str)
{
   int index;

   index = 0;
   evas_common_font_utf8_get_next((unsigned char *)str, &index);
   if (index >= 0)
     {
	str = str + index;
	_layout_item_text_cutoff(c, it, index);
     }
   else
     str = NULL;
   return str;
}
#endif

static int
_layout_last_item_ends_in_whitespace(Ctxt *c)
{
   Evas_Object_Textblock_Item *it;

   if (!c->ln->items) return 1;
   it = (Evas_Object_Textblock_Item *)(EINA_INLIST_GET(c->ln->items))->last;
   return _layout_ends_with_space(it->text);
}

static int
_layout_word_end(char *str, int p)
{
   int ch, tp;

   tp = p;
   ch = evas_common_font_utf8_get_next((unsigned char *)str, &tp);
   while ((!_is_white(ch)) && (tp >= 0) && (ch != 0))
     {
	p = tp;
	ch = evas_common_font_utf8_get_next((unsigned char *)str, &tp);
     }
   if (ch == 0) return -1;
   return p;
}

static int
_layout_word_next(char *str, int p)
{
   int ch, tp;

   tp = p;
   ch = evas_common_font_utf8_get_next((unsigned char *)str, &tp);
   while ((!_is_white(ch)) && (tp >= 0) && (ch != 0))
     {
	p = tp;
	ch = evas_common_font_utf8_get_next((unsigned char *)str, &tp);
     }
   if (ch == 0) return -1;
   while ((_is_white(ch)) && (tp >= 0) && (ch != 0))
     {
	p = tp;
	ch = evas_common_font_utf8_get_next((unsigned char *)str, &tp);
     }
   if (ch == 0) return -1;
   return p;
}

static void
_layout_walk_back_to_item_word_redo(Ctxt *c, Evas_Object_Textblock_Item *it)
{
   Evas_Object_Textblock_Item *pit, *new_it = NULL;
   Eina_List *remove_items = NULL, *l;
   Eina_Inlist *data;
   int index, tw, th, inset, adv;

   /* it is not appended yet */
   EINA_INLIST_REVERSE_FOREACH((EINA_INLIST_GET(c->ln->items)), pit)
     {
	if (_layout_ends_with_space(pit->text))
	  {
	     break;
	  }
	index = evas_common_font_utf8_get_last((unsigned char *)(pit->text), strlen(pit->text));
	index = _layout_word_start(pit->text, index);
	if (index == 0)
	  remove_items = eina_list_prepend(remove_items, pit);
	else
	  {
	     new_it = _layout_item_new(c, pit->format, pit->text + index);
	     new_it->source_node = pit->source_node;
	     new_it->source_pos = pit->source_pos + index;
	     _layout_item_text_cutoff(c, pit, index);
	     _layout_strip_trailing_whitespace(c, pit->format, pit);
	     break;
	  }
     }
   EINA_LIST_FOREACH(remove_items, l, data)
     c->ln->items = (Evas_Object_Textblock_Item *)eina_inlist_remove(EINA_INLIST_GET(c->ln->items), data);
   /* new line now */
   if (remove_items)
     {
	pit = remove_items->data;
	_layout_line_advance(c, pit->format);
     }
   else
     {
	_layout_line_advance(c, it->format);
     }
   if (new_it)
     {
	/* append new_it */
	tw = th = 0;
	if (new_it->format->font.font)
	  c->ENFN->font_string_size_get(c->ENDT, new_it->format->font.font, new_it->text, &tw, &th);
	new_it->w = tw;
	new_it->h = th;
	inset = 0;
	if (new_it->format->font.font)
	  inset = c->ENFN->font_inset_get(c->ENDT, new_it->format->font.font, new_it->text);
	new_it->inset = inset;
	new_it->x = c->x;
	adv = 0;
	if (new_it->format->font.font)
	  adv = c->ENFN->font_h_advance_get(c->ENDT, new_it->format->font.font, new_it->text);
	c->x += adv;
	c->ln->items = (Evas_Object_Textblock_Item *)eina_inlist_append(EINA_INLIST_GET(c->ln->items), EINA_INLIST_GET(new_it));
     }
   while (remove_items)
     {
	pit = remove_items->data;
	remove_items = eina_list_remove_list(remove_items, remove_items);
	/* append pit */
	pit->x = c->x;
	adv = c->ENFN->font_h_advance_get(c->ENDT, pit->format->font.font, pit->text);
	c->x += adv;
	c->ln->items = (Evas_Object_Textblock_Item *)eina_inlist_append(EINA_INLIST_GET(c->ln->items), EINA_INLIST_GET(pit));
     }
   if (it)
     {
	/* append it */
	it->x = c->x;
	adv = 0;
	if (it->format->font.font)
	  adv = c->ENFN->font_h_advance_get(c->ENDT, it->format->font.font, it->text);
	c->x += adv;
	c->ln->items = (Evas_Object_Textblock_Item *)eina_inlist_append(EINA_INLIST_GET(c->ln->items), EINA_INLIST_GET(it));
     }
}

static void
_layout_text_append(Ctxt *c, Evas_Object_Textblock_Format *fmt, Evas_Object_Textblock_Node *n, const char *repch)
{
   int adv, inset, tw, th, new_line, empty_item;
   int wrap, twrap, ch, index, white_stripped;
   char *str;
   const char *tbase;
   Evas_Object_Textblock_Item *it;

   /*
    * If there is replacement character and text in node is non-empty, replace
    * text with replacement characters.
    */
   if (n)
     {
        if ((repch) && (eina_strbuf_length_get(n->text)))
          {
             int i, len, chlen;
             char *ptr;
             
             len = evas_common_font_utf8_get_len((unsigned char *) eina_strbuf_string_get(n->text));
             chlen = strlen(repch);
             str = alloca((len * chlen) + 1);
             for (i = 0, ptr = str; i < len; ptr += chlen, i++)
               memcpy(ptr, repch, chlen);
             *ptr = 0;
          }
        else
             str = (char *)eina_strbuf_string_get(n->text);
     }
   else
        str = "";
    
   /* Keep beginning of string to calculate offsets later */
   tbase = str;

//   printf("add: wrap: %i|%i, width: %i '%s'\n", fmt->wrap_word, fmt->wrap_char, c->w, str);

   /*
    * Layout the string piece-by-piece
    */
   new_line = 0;
   empty_item = 0;
   while (str)
     {
        /* FIXME: next comment is probably obsolete */
	/* if this is the first line item and it starts with spaces - remove them */
	wrap = 0;
	white_stripped = 0;
/*
	if (!c->ln->items)
	  {
	     twrap = wrap;
	     ch = evas_common_font_utf8_get_next((unsigned char *)str, &wrap);
	     while (_is_white(ch))
	       {
		  twrap = wrap;
		  ch = evas_common_font_utf8_get_next((unsigned char *)str, &wrap);
	       }
	     str = str + twrap;
	  }
 */
        /*
         * Create new Textblock_Item for current part of string and format it
         * with current format
         *
         * FIXME: why not fmt is attached to Textblock_Node instead?
         */
	it = _layout_item_new(c, fmt, str);

        /*
         * Fill Textblock_Item with data pointing back to Textblock_Node
         */
	it->source_node = n;
	it->source_pos = str - tbase;

        /* Get width/height of text being rasterized "as if there were no constraints" */
	tw = th = 0;
	if (fmt->font.font) {
	  c->ENFN->font_string_size_get(c->ENDT, fmt->font.font, it->text, &tw, &th);
        }

        /*
         * Split text to fill the current line
         */
	if ((c->w >= 0) &&
	    ((fmt->wrap_word) || (fmt->wrap_char) || fmt->ellipsis_left || fmt->ellipsis_right) &&
	    ((c->x + tw) >
	     (c->w - c->o->style_pad.l - c->o->style_pad.r -
	      c->marginl - c->marginr)))
	  {
             /*
              * This branch: text needs to be wrapped/ellipsized, and does not fit single
              * line.
              */
              if (fmt->ellipsis_left || fmt->ellipsis_right) {

                  int width = c->w - c->o->style_pad.l - c->o->style_pad.r - c->marginl - c->marginr - c->x;

                  char *ellipsis_symbol = fmt->ellipsis_symbol ? fmt->ellipsis_symbol : "...";

                  int ew, eh;
                  c->ENFN->font_string_size_get(c->ENDT, fmt->font.font, ellipsis_symbol, &ew, &eh);

                  /* Now we know size of ellipsis character, so we can calculate how much we want to cut */

                  if (fmt->ellipsis_right) {
                      int cut = c->ENFN->font_last_up_to_pos(c->ENDT, fmt->font.font, it->text,
                                                              width - ew, 0);

                      char *ts = it->text;
                      asprintf(&it->text, "%.*s%s", cut, ts, ellipsis_symbol);
                      free(ts);
                  } else {
                      int cut = evas_common_font_query_suffix_needed_width(fmt->font.font, it->text,
                                                                           width - ew);

                      char *ts = it->text;
                      asprintf(&it->text, "%s%s", ellipsis_symbol, ts + cut);
                      free(ts);
                  }

                  str = NULL;
                  new_line = EINA_TRUE;

              } else {
                  wrap = _layout_text_cutoff_get(c, fmt, it);
                  if (wrap == 0)
                      evas_common_font_utf8_get_next((unsigned char *)str, &wrap);

                  if (wrap > 0)
                  {
                      if (fmt->wrap_word)
                      {
                          index = wrap;
                          ch = evas_common_font_utf8_get_next((unsigned char *)str, &index);
                          if (!_is_white(ch))
                              wrap = _layout_word_start(str, wrap);
                          if (wrap > 0)
                          {
                              twrap = wrap;
                              ch = evas_common_font_utf8_get_prev((unsigned char *)str, &twrap);
                              /* the text intersects the wrap point on a whitespace char */
                              if (_is_white(ch))
			      {
                                  _layout_item_text_cutoff(c, it, wrap);
                                  twrap = wrap;
                                  /*we don't want to move next, that's why it's
                                   * commented out.
                                   * ch = evas_common_font_utf8_get_next((unsigned char *)str, &twrap);
                                   */
                                  str = str + twrap;
			      }
                              /* intersects a word */
                              else
			      {
                                  /* walk back to start of word */
                                  twrap = _layout_word_start(str, wrap);
                                  if (twrap != 0)
                                  {
				      wrap = twrap;
				      ch = evas_common_font_utf8_get_prev((unsigned char *)str, &twrap);
				      _layout_item_text_cutoff(c, it, twrap);
				      str = str + wrap;
                                  }
                                  else
                                  {
				      empty_item = 1;
				      if (it->text) free(it->text);
				      _format_free(c->obj, it->format);
				      free(it);
                                      if (c->ln->items)
                                        {
                                           it = (Evas_Object_Textblock_Item *)(EINA_INLIST_GET(c->ln->items))->last;
                                           _layout_strip_trailing_whitespace(c, fmt, it);
                                           twrap = _layout_word_end(str, wrap);
					   if (twrap >= 0)
					     {
						ch = evas_common_font_utf8_get_next((unsigned char *)str, &twrap);
						str = str + twrap;
					     }
					   else
					     str = NULL;
                                        }
				   }
			      }
                          }
                          else
                          {
                              /* wrap now is the index of the word START */
                              index = wrap;
                              ch = evas_common_font_utf8_get_next((unsigned char *)str, &index);
                              if (!_is_white(ch) &&
                                  (!_layout_last_item_ends_in_whitespace(c)))
			      {
                                  _layout_walk_back_to_item_word_redo(c, it);
                                  return;
			      }
                              if (c->ln->items != NULL)
			      {
                                  white_stripped = _layout_item_abort(c, fmt, it);
                                  empty_item = 1;
			      }
                              else
			      {
                                  if (wrap <= 0)
                                  {
				      wrap = 0;
				      twrap = _layout_word_end(it->text, wrap);
				      wrap = twrap;
				      if (twrap >= 0)
					{
					   ch = evas_common_font_utf8_get_next((unsigned char *)str, &wrap);
					   _layout_item_text_cutoff(c, it, twrap);
					}
				      if (wrap > 0)
                                          str = str + wrap;
				      else
                                          str = NULL;
                                  }
                                  else
                                      str = NULL;
			      }
                          }
                      }
                      else if (fmt->wrap_char)
                      {
                          _layout_item_text_cutoff(c, it, wrap);
                          str = str + wrap;
                      }
                      new_line = 1;
                  }
                  else
                  {
                      /* wrap now is the index of the word START */
                      if (wrap <= 0)
                      {
                          if (wrap < 0) wrap = 0;
                          index = wrap;
                          ch = evas_common_font_utf8_get_next((unsigned char *)str, &index);
                          if (!_is_white(ch) &&
                              (!_layout_last_item_ends_in_whitespace(c)))
                          {
                              _layout_walk_back_to_item_word_redo(c, it);
                              return;
                          }
                      }
                      if (c->ln->items != NULL)
                      {
                          white_stripped = _layout_item_abort(c, fmt, it);
                          empty_item = 1;
                          new_line = 1;
                      }
                      else
                      {
                          if (wrap <= 0)
                          {
                              wrap = 0;
                              twrap = _layout_word_end(it->text, wrap);
                              wrap = _layout_word_next(it->text, wrap);
                              if (twrap >= 0)
                                  _layout_item_text_cutoff(c, it, twrap);
                              if (wrap >= 0)
                                  str = str + wrap;
                              else
                                  str = NULL;
                          }
                          else
                              str = NULL;
                          new_line = 1;
                      }
                  }
              }

              if (!empty_item)
              {
                  tw = th = 0;
                  if (fmt->font.font)
                      c->ENFN->font_string_size_get(c->ENDT, fmt->font.font, it->text, &tw, &th);
              }
	  }
	else
          {
             /*
              * Either string is empty, or line wrapping was not requested or
              * string fits the line.  Anyway, just process this line and break
              * out of loop.
              */
             str = NULL;
          }

        /* Handle non-empty items */
	if (!empty_item)
	  {
             /*
              * Store width and height of Textblock_Item as got from
              * font_string_size_get
              */
	     it->w = tw;
	     it->h = th;

             /* Adjust for insets. Will be useful later while drawing characters */
	     inset = 0;
	     if (fmt->font.font)
	       inset = c->ENFN->font_inset_get(c->ENDT, fmt->font.font, it->text);
	     it->inset = inset;

             /*
              * Store horizontal position of Textblock_Item and adjust c->x
              * according to data retrieved from font system.
              *
              * FIXME: why it->h might be != adv?
              */
	     it->x = c->x;
	     adv = 0;
	     if (fmt->font.font)
	       adv = c->ENFN->font_h_advance_get(c->ENDT, fmt->font.font, it->text);
	     c->x += adv;

             /* Add the Textblock_Item to items in current line */
	     c->ln->items = (Evas_Object_Textblock_Item *)eina_inlist_append(EINA_INLIST_GET(c->ln->items), EINA_INLIST_GET(it));
	  }
        empty_item = 0;

        /* Handle "newline requested" */
	if (new_line)
	  {
              /* Skip single (?) whitespace character from str */
	     if (str)
	       {
		  if (!white_stripped)
		    {
		       index = 0;
		       ch = evas_common_font_utf8_get_next((unsigned char *)str, &index);
		       if (_is_white(ch)) str += index;
		    }
	       }
             /* Format current line and advance to new one */
	     _layout_line_advance(c, fmt);
	  }
        new_line = 0;
     }
}

static Evas_Object_Textblock_Format_Item *
_layout_format_item_add(Ctxt *c, Evas_Object_Textblock_Node *n, char *item)
{
   Evas_Object_Textblock_Format_Item *fi;

   fi = calloc(1, sizeof(Evas_Object_Textblock_Format_Item));
   fi->item = eina_stringshare_add(item);
   fi->source_node = n;
   c->ln->format_items = (Evas_Object_Textblock_Format_Item *)eina_inlist_append(EINA_INLIST_GET(c->ln->format_items),
										 EINA_INLIST_GET(fi));
   return fi;
}

static void
_layout(const Evas_Object *obj, int calc_only, int w, int h, int *w_ret, int *h_ret)
{
   Evas_Object_Textblock *o;
   Ctxt ctxt, *c;
   Evas_Object_Textblock_Line *ln;
   Evas_Object_Textblock_Node *n;
   Eina_List *removes = NULL;
   Evas_Object_Textblock_Format *fmt = NULL;
   int style_pad_l = 0, style_pad_r = 0, style_pad_t = 0, style_pad_b = 0;

   /* setup context */
   o = (Evas_Object_Textblock *)(obj->object_data);
   c = &ctxt;
   c->obj = (Evas_Object *)obj;
   c->o = o;
   c->lines = c->ln = NULL;
   c->format_stack = NULL;
   c->x = c->y = 0;
   c->w = w;
   c->h = h;
   c->wmax = c->hmax = 0;
   c->marginl = c->marginr = 0;
   c->have_underline = 0;
   c->have_underline2 = 0;
   c->underline_extend = 0;
   c->line_no = 0;
   c->align = 0.0;

   /* Calculate maxascent, maxdescent for current line */
   int maxascent = 0;
   int maxdescent = 0;

   _format_command_init();
   /* setup default base style */
   if ((c->o->style) && (c->o->style->default_tag))
     {
	fmt = _layout_format_push(c, NULL);
	_format_fill(c->obj, fmt, c->o->style->default_tag);
     }
   if (!fmt)
     {
	_format_command_shutdown();
	if (w_ret) *w_ret = 0;
	if (h_ret) *h_ret = 0;
	return;
     }
   /* run through all text and format nodes generating lines */
   if (!c->o->nodes)
     {
        if (!c->ln)
          {
             _layout_line_new(c, fmt);
             _layout_text_append(c, fmt, NULL, NULL);
             _layout_line_advance(c, fmt);
//             printf("bl:%i | %ix%i\n", c->ln->baseline, c->ln->w, c->ln->h);
             /*
             tw = th = 0;
             if (fmt->font.font)
               {
                  c->ENFN->font_string_size_get(c->ENDT, fmt->font.font, "", &tw, &th);
                  c->ln->x = 0;
                  c->ln->y = 0;
                  c->ln->w = tw;
                  c->ln->h = th;
                  c->wmax = tw;
                  c->hmax = th;
                  _layout_format_ascent_descent_adjust(c, fmt);
               }
              */
          }
     }

   _layout_format_ascent_descent_adjust(c, fmt, &maxascent, &maxdescent);

   EINA_INLIST_FOREACH(c->o->nodes, n)
     {
	if (!c->ln) _layout_line_new(c, fmt);
	if ((n->type == NODE_FORMAT) && eina_strbuf_length_get(n->text))
	  {
	     char *s;
	     char *item;
             int handled = 0;

	     s = (char *)eina_strbuf_string_get(n->text);
             if (!strncmp(s, "+ item ", 7))
               {
                  // one of:
                  //   item size=20x10 href=name
                  //   item relsize=20x10 href=name
                  //   item abssize=20x10 href=name
                  // 
                  // optional arguments:
                  //   vsize=full
                  //   vsize=ascent
                  // 
                  // size == item size (modifies line size) - can be multiplied by
                  //   scale factor
                  // relsize == relative size (height is current font height, width
                  //   modified accordingly keeping aspect)
                  // abssize == absolute size (modifies line size) - never mulitplied by
                  //   scale factor
                  // href == name of item - to be found and matched later and used for
                  //   positioning
                  Evas_Object_Textblock_Format_Item *fi;
                  int x2, w = 1, h = 1;
                  int vsize = 0, size = 0;
                  char *p;
                  
                  // don't care
                  //href = strstr(s, " href=");
                  p = strstr(s, " vsize=");
                  if (p)
                    {
                       p += 7;
                       if (!strncmp(p, "full", 4)) vsize = VSIZE_FULL;
                       else if (!strncmp(p, "ascent", 6)) vsize = VSIZE_ASCENT;
                    }
                  p = strstr(s, " size=");
                  if (p)
                    {
                       p += 6;
                       if (sscanf(p, "%ix%i", &w, &h) == 2)
                         {
                            w = w * obj->cur.scale;
                            h = h * obj->cur.scale;
                            size = SIZE;
                         }
                    }
                  else
                    {
                       p = strstr(s, " absize=");
                       if (p)
                         {
                            p += 8;
                            if (sscanf(p, "%ix%i", &w, &h) == 2)
                              {
                                 size = SIZE_ABS;
                              }
                         }
                       else
                         {
                            p = strstr(s, " relsize=");
                            if (p)
                              {
                                 p += 9;
                                 if (sscanf(p, "%ix%i", &w, &h) == 2)
                                   {
                                      int sz = 1;
                                      size = SIZE_REL;
                                      if (vsize == VSIZE_FULL)
                                        {
                                           sz = maxdescent + maxascent;
                                        }
                                      else if (vsize == VSIZE_ASCENT)
                                        {
                                           sz = maxascent;
                                        }
                                      w = (w * sz) / h;
                                      h = sz;
                                   }
                              }
                         }
                    }

                  x2 = c->x + w;
                  
                  if (x2 >
                      (c->w - c->o->style_pad.l -
                       c->o->style_pad.r -
                       c->marginl - c->marginr))
                    {
                       _layout_line_advance(c, fmt);
                       x2 = w;
                    }
                  fi = _layout_format_item_add(c, n, item);
                  fi->x = c->x;
                  fi->vsize = vsize;
                  fi->size = size;
                  fi->formatme = 1;
                  fi->w = w;
                  fi->h = h;
                  fi->ascent = maxascent;
                  fi->descent = maxdescent;
                  c->x = x2;
                  handled = 1;
               }
             if (!handled)
               {
                  if (s[0] == '+')
                    {
                       fmt = _layout_format_push(c, fmt);
                       s++;
                    }
                  else if (s[0] == '-')
                    {
                       fmt = _layout_format_pop(c, fmt);
                       s++;
                    }
                  while ((item = _format_parse(&s)))
                    {
                       char tmp_delim = *s;
                       *s = '\0';
                       if (_format_is_param(item))
                         {
                            _layout_format_value_handle(c, fmt, item);
                         }
                       else
                         {
                            if ((!strcmp(item, "\n")) || (!strcmp(item, "\\n")))
                              {
                                 Evas_Object_Textblock_Format_Item *fi;
                                 
                                 fi = _layout_format_item_add(c, n, item);
                                 fi->x = c->x;
                                 fi->w = 0;
                                 _layout_line_advance(c, fmt);
                              }
                            else if ((!strcmp(item, "\t")) || (!strcmp(item, "\\t")))
                              {
                                 Evas_Object_Textblock_Format_Item *fi;
                                 int x2;
                                 
                                 x2 = (fmt->tabstops * ((c->x + fmt->tabstops) / fmt->tabstops));
                                 if (x2 >
                                     (c->w - c->o->style_pad.l -
                                      c->o->style_pad.r -
                                      c->marginl - c->marginr))
                                   {
                                      _layout_line_advance(c, fmt);
                                      x2 = (fmt->tabstops * ((c->x + fmt->tabstops) / fmt->tabstops));
                                   }
                                 if (c->ln->items)
                                   {
                                      Evas_Object_Textblock_Item *it;
                                      
                                      it = (Evas_Object_Textblock_Item *)(EINA_INLIST_GET(c->ln->items))->last;
                                      _layout_strip_trailing_whitespace(c, fmt, it);
                                   }
                                 fi = _layout_format_item_add(c, n, item);
                                 fi->x = c->x;
                                 fi->w = x2 - c->x;
                                 c->x = x2;
                              }
                         }
                       *s = tmp_delim;
                    }
               }
             
	     evas_text_style_pad_get(fmt->style, &style_pad_l, &style_pad_r, &style_pad_t, &style_pad_b);

	     if (fmt->underline2)
	       c->have_underline2 = 1;
	     else if (fmt->underline)
	       c->have_underline = 1;
	  }
	else if ((n->type == NODE_TEXT) && eina_strbuf_length_get(n->text))
	  {
	     _layout_text_append(c, fmt, n, o->repch);
	     if ((c->have_underline2) || (c->have_underline))
	       {
		  if (style_pad_b < c->underline_extend)
		    style_pad_b = c->underline_extend;
		  c->have_underline = 0;
		  c->have_underline2 = 0;
		  c->underline_extend = 0;
	       }
	  }
     }
   if ((c->ln) && (c->ln->items) && (fmt))
     _layout_line_advance(c, fmt);
   while (c->format_stack)
     {
	fmt = c->format_stack->data;
	c->format_stack = eina_list_remove_list(c->format_stack, c->format_stack);
	_format_free(c->obj, fmt);
     }
   EINA_INLIST_FOREACH(c->lines, ln)
     {
	if (ln->line_no == -1)
	  {
	     removes = eina_list_append(removes, ln);
	  }
	else
	  {
	     if ((ln->y + ln->h) > c->hmax) c->hmax = ln->y + ln->h;
	  }
     }
   while (removes)
     {
	ln = removes->data;
	c->lines = (Evas_Object_Textblock_Line *)eina_inlist_remove(EINA_INLIST_GET(c->lines), EINA_INLIST_GET(ln));
	removes = eina_list_remove_list(removes, removes);
	_line_free(obj, ln);
     }

   if (w_ret) *w_ret = c->wmax;
   if (h_ret) *h_ret = c->hmax;
   if ((o->style_pad.l != style_pad_l) || (o->style_pad.r != style_pad_r) ||
       (o->style_pad.t != style_pad_t) || (o->style_pad.b != style_pad_b))
     {
	Evas_Object_Textblock_Line *lines;

	lines = c->lines;
	c->lines = NULL;
	o->style_pad.l = style_pad_l;
	o->style_pad.r = style_pad_r;
	o->style_pad.t = style_pad_t;
	o->style_pad.b = style_pad_b;
	_layout(obj, calc_only, w, h, w_ret, h_ret);
        _lines_clear(obj, lines);
	_format_command_shutdown();
	return;
     }
   if (!calc_only)
     {
	o->lines = c->lines;
	_format_command_shutdown();
	return;
     }
   if (c->lines) _lines_clear(obj, c->lines);
   _format_command_shutdown();
}

static void
_relayout(const Evas_Object *obj)
{
   Evas_Object_Textblock *o;
   Evas_Object_Textblock_Line *lines;

   o = (Evas_Object_Textblock *)(obj->object_data);
   lines = o->lines;
   o->lines = NULL;
   o->formatted.valid = 0;
   o->native.valid = 0;
   _layout(obj,
	   0,
	   obj->cur.geometry.w, obj->cur.geometry.h,
	   &o->formatted.w, &o->formatted.h);
   o->formatted.valid = 1;
   if (lines) _lines_clear(obj, lines);
   o->last_w = obj->cur.geometry.w;
   o->changed = 0;
   o->redraw = 1;
}

static void
_find_layout_item_line_match(Evas_Object *obj, Evas_Object_Textblock_Node *n, int pos, int eol, Evas_Object_Textblock_Line **lnr, Evas_Object_Textblock_Item **itr)
{
   Evas_Object_Textblock_Line *ln;
/*    Evas_Object_Textblock_Node *nn; */
   Evas_Object_Textblock *o;

   o = (Evas_Object_Textblock *)(obj->object_data);
/*    EINA_INLIST_FOREACH(o->nodes, nn) */
/*      ; */
   if ((eol) && (n->type == NODE_TEXT))
     {
        int pos2 = pos;

        evas_common_font_utf8_get_prev((unsigned char *) eina_strbuf_string_get(n->text), &pos2);
        if (pos2 < pos) pos = pos2;
     }
   EINA_INLIST_FOREACH(o->lines, ln)
     {
	Evas_Object_Textblock_Format_Item *fit;
	Evas_Object_Textblock_Item *it;
        Evas_Object_Textblock_Line *lnn;

        lnn = (Evas_Object_Textblock_Line *)(((Eina_Inlist *)ln)->next);
	EINA_INLIST_FOREACH(ln->items, it)
	  {
	     if (it->source_node == n)
	       {
                  Evas_Object_Textblock_Item *itn;
                  int p;

                  itn = (Evas_Object_Textblock_Item *)(((Eina_Inlist *)it)->next);
                  p = (int)(it->source_pos + strlen(it->text));
		  if ((p >= pos) ||
                      ((p == pos) && (!lnn) &&
                       ((!itn)  |
                        ((itn) && (itn->source_node != n)))))
		    {
		       *lnr = ln;
		       *itr = it;
		       return;
		    }
	       }
	  }
	EINA_INLIST_FOREACH(ln->format_items, fit)
	  {
	     if (fit->source_node == n)
	       {
		  *lnr = ln;
		  /* FIXME: Is that really what we want ? */
		  *itr = (Evas_Object_Textblock_Item *)fit;
		  return;
	       }
	  }
     }
}

static void
_find_layout_format_item_line_match(Evas_Object *obj, Evas_Object_Textblock_Node *n, Evas_Object_Textblock_Line **lnr, Evas_Object_Textblock_Format_Item **fir)
{
   Evas_Object_Textblock_Line *ln;
   Evas_Object_Textblock *o;

   o = (Evas_Object_Textblock *)(obj->object_data);
   EINA_INLIST_FOREACH(o->lines, ln)
     {
	Evas_Object_Textblock_Format_Item *fi;

	EINA_INLIST_FOREACH(ln->format_items, fi)
	  {
	     if (fi->source_node == n)
	       {
		  *lnr = ln;
		  *fir = fi;
		  return;
	       }
	  }
     }
}

static Evas_Object_Textblock_Line *
_find_layout_line_num(const Evas_Object *obj, int line)
{
   Evas_Object_Textblock_Line *ln;
   Evas_Object_Textblock *o;

   o = (Evas_Object_Textblock *)(obj->object_data);
   EINA_INLIST_FOREACH(o->lines, ln)
     {
	if (ln->line_no == line) return ln;
     }
   return NULL;
}

/**
 * Adds a textblock to the given evas.
 * @param   e The given evas.
 * @return  The new textblock object.
 */
EAPI Evas_Object *
evas_object_textblock_add(Evas *e)
{
   Evas_Object *obj;

   MAGIC_CHECK(e, Evas, MAGIC_EVAS);
   return NULL;
   MAGIC_CHECK_END();
   obj = evas_object_new(e);
   evas_object_textblock_init(obj);
   evas_object_inject(obj, e);
   return obj;
}

/**
 * Creates a new textblock style.
 * @return  The new textblock style.
 */
EAPI Evas_Textblock_Style *
evas_textblock_style_new(void)
{
   Evas_Textblock_Style *ts;

   ts = calloc(1, sizeof(Evas_Textblock_Style));
   return ts;
}

/**
 * Destroys a textblock style.
 * @param ts The textblock style to free.
 */
EAPI void
evas_textblock_style_free(Evas_Textblock_Style *ts)
{
   if (!ts) return;
   if (ts->objects)
     {
	ts->delete_me = 1;
	return;
     }
   _style_clear(ts);
   free(ts);
}

/**
 * to be documented.
 * @param ts  to be documented.
 * @param text  to be documented.
 * @return Returns no value.
 */
EAPI void
evas_textblock_style_set(Evas_Textblock_Style *ts, const char *text)
{
   Eina_List *l;
   Evas_Object *obj;

   if (!ts) return;

   EINA_LIST_FOREACH(ts->objects, l, obj)
     {
	Evas_Object_Textblock *o;

	o = (Evas_Object_Textblock *)(obj->object_data);
	if (o->markup_text)
	  {
	     free(o->markup_text);
	     o->markup_text = NULL;
	     evas_object_textblock_text_markup_get(obj);
	  }
     }

   _style_clear(ts);
   if (text) ts->style_text = strdup(text);

   if (ts->style_text)
     {
	// format MUST be KEY='VALUE'[KEY='VALUE']...
	char *p;
	char *key_start, *key_stop, *val_start, *val_stop;

	key_start = key_stop = val_start = val_stop = NULL;
	p = ts->style_text;
	while (*p)
	  {
	     if (!key_start)
	       {
		  if (!isspace(*p))
		    key_start = p;
	       }
	     else if (!key_stop)
	       {
		  if ((*p == '=') || (isspace(*p)))
		    key_stop = p;
	       }
	     else if (!val_start)
	       {
		  if (((*p) == '\'') && (*(p + 1)))
		    val_start = p + 1;
	       }
	     else if (!val_stop)
	       {
		  if (((*p) == '\'') && (p > ts->style_text) && (p[-1] != '\\'))
		    val_stop = p;
	       }
	     if ((key_start) && (key_stop) && (val_start) && (val_stop))
	       {
		  char *tags, *replaces;
		  Evas_Object_Style_Tag *tag;
		  size_t tag_len = key_stop - key_start;
		  size_t replace_len = val_stop - val_start;

		  tags = malloc(tag_len + 1);
		  if (tags)
		    {
		       memcpy(tags, key_start, tag_len);
		       tags[tag_len] = 0;
		    }

		  replaces = malloc(replace_len + 1);
		  if (replaces)
		    {
		       memcpy(replaces, val_start, replace_len);
		       replaces[replace_len] = 0;
		    }
		  if ((tags) && (replaces))
		    {
		       if (!strcmp(tags, "DEFAULT"))
			 {
			    ts->default_tag = replaces;
			    free(tags);
			 }
		       else
			 {
			    tag = calloc(1, sizeof(Evas_Object_Style_Tag));
			    if (tag)
			      {
				 tag->tag = tags;
				 tag->replace = replaces;
				 tag->tag_len = tag_len;
				 tag->replace_len = replace_len;
				 ts->tags = (Evas_Object_Style_Tag *)eina_inlist_append(EINA_INLIST_GET(ts->tags), EINA_INLIST_GET(tag));
			      }
			    else
			      {
				 free(tags);
				 free(replaces);
			      }
			 }
		    }
		  else
		    {
		       if (tags) free(tags);
		       if (replaces) free(replaces);
		    }
		  key_start = key_stop = val_start = val_stop = NULL;
	       }
	     p++;
	  }
     }

   EINA_LIST_FOREACH(ts->objects, l, obj)
     {
	Evas_Object_Textblock *o;

	o = (Evas_Object_Textblock *)(obj->object_data);
	if (o->markup_text)
	  {
	     char *m;

	     m = strdup(o->markup_text);
	     if (m)
	       {
		  evas_object_textblock_text_markup_set(obj, m);
		  free(m);
	       }
	  }
     }
}

/**
 * to be documented.
 * @param ts  to be documented.
 * @return to be documented.
 */
EAPI const char *
evas_textblock_style_get(const Evas_Textblock_Style *ts)
{
   if (!ts) return NULL;
   return ts->style_text;
}

/* textblock styles */
/**
 * to be documented.
 * @param obj to be documented.
 * @param ts  to be documented.
 * @return Returns no value.
 */
EAPI void
evas_object_textblock_style_set(Evas_Object *obj, Evas_Textblock_Style *ts)
{
   TB_HEAD();
   if (ts == o->style) return;
   if ((ts) && (ts->delete_me)) return;
   if (o->markup_text)
     {
        if (o->style)
          {
             free(o->markup_text);
             o->markup_text = NULL;
             evas_object_textblock_text_markup_get(obj);
          }
     }
   if (o->style)
     {
	Evas_Textblock_Style *old_ts;

	old_ts = o->style;
	old_ts->objects = eina_list_remove(old_ts->objects, obj);
	if ((old_ts->delete_me) && (!old_ts->objects))
	  evas_textblock_style_free(old_ts);
     }
   if (ts)
     {
	ts->objects = eina_list_append(ts->objects, obj);
	o->style = ts;
     }
   else
     {
	o->style = NULL;
     }

   o->formatted.valid = 0;
   o->native.valid = 0;
   o->changed = 1;
   if (o->markup_text)
     {
	free(o->markup_text);
	o->markup_text = NULL;
     }
   evas_object_change(obj);
}

/**
 * to be documented.
 * @param obj  to be documented.
 * @return to be documented.
 */
EAPI const Evas_Textblock_Style *
evas_object_textblock_style_get(const Evas_Object *obj)
{
   TB_HEAD_RETURN(NULL);
   return o->style;
}

/**
 * @brief Set the "replacement character" to use for the given textblock object.
 *
 * @param obj The given textblock object.
 * @param ch The charset name.
 */
EAPI void
evas_object_textblock_replace_char_set(Evas_Object *obj, const char *ch)
{
   TB_HEAD();
   if (o->repch) eina_stringshare_del(o->repch);
   if (ch) o->repch = eina_stringshare_add(ch);
   else o->repch = NULL;
   o->formatted.valid = 0;
   o->native.valid = 0;
   o->changed = 1;
   if (o->markup_text)
     {
	free(o->markup_text);
	o->markup_text = NULL;
     }
   evas_object_change(obj);
}

/**
 * @brief Get the "replacement character" for given textblock object. Returns
 * NULL if no replacement character is in use.
 *
 * @param obj The given textblock object
 * @return replacement character or NULL
 */
EAPI const char *
evas_object_textblock_replace_char_get(Evas_Object *obj)
{
   TB_HEAD_RETURN(NULL);
   return o->repch;
}


static inline void
_advance_after_end_of_string(const char **p_buf)
{
   while (**p_buf != 0) (*p_buf)++;
   (*p_buf)++;
}

static inline int
_is_eq_and_advance(const char *s, const char *s_end,
		   const char **p_m, const char *m_end)
{
   for (;((s < s_end) && (*p_m < m_end)); s++, (*p_m)++)
     {
	if (*s != **p_m)
	  {
	     _advance_after_end_of_string(p_m);
	     return 0;
	  }
     }

   if (*p_m < m_end)
     _advance_after_end_of_string(p_m);

   return s == s_end;
}

static inline const char *
_escaped_char_match(const char *s, int *adv)
{
   const char *map_itr, *map_end, *mc, *sc;

   map_itr = escape_strings;
   map_end = map_itr + sizeof(escape_strings);

   while (map_itr < map_end)
     {
	const char *escape;
	int match;

	escape = map_itr;
	_advance_after_end_of_string(&map_itr);
	if (map_itr >= map_end) break;

	mc = map_itr;
	sc = s;
	match = 1;
	while ((*mc) && (*sc))
	  {
	     if ((unsigned char)*sc < (unsigned char)*mc) return NULL;
	     if (*sc != *mc) match = 0;
	     mc++;
	     sc++;
	  }
	if (match)
	  {
	     *adv = mc - map_itr;
	     return escape;
	  }
	_advance_after_end_of_string(&map_itr);
     }
   return NULL;
}

static inline const char *
_escaped_char_get(const char *s, const char *s_end)
{
   const char *map_itr, *map_end;

   map_itr = escape_strings;
   map_end = map_itr + sizeof(escape_strings);

   while (map_itr < map_end)
     {
	if (_is_eq_and_advance(s, s_end, &map_itr, map_end))
	  return map_itr;
	if (map_itr < map_end)
	  _advance_after_end_of_string(&map_itr);
     }
   return NULL;
}

/**
 * to be documented.
 * @param escape to be documented.
 * @return to be documented.
 */
EAPI const char *
evas_textblock_escape_string_get(const char *escape)
{
   /* &amp; -> & */
   return _escaped_char_get(escape, escape + strlen(escape));
}

/**
 * to be documented.
 * @param escape_start to be documented.
 * @param escape_end to be documented.
 * @return to be documented.
 */
EAPI const char *
evas_textblock_escape_string_range_get(const char *escape_start, const char *escape_end)
{
   return _escaped_char_get(escape_start, escape_end);
}

/**
 * to be documented.
 * @param string to be documented.
 * @param len_ret to be documented.
 * @return to be documented.
 */
EAPI const char *
evas_textblock_string_escape_get(const char *string, int *len_ret)
{
   /* & -> &amp; */
   return _escaped_char_match(string, len_ret);
}

static inline void
_append_escaped_char(Evas_Textblock_Cursor *cur, const char *s,
		     const char *s_end)
{
   const char *escape;

   escape = _escaped_char_get(s, s_end);
   if (escape)
     evas_textblock_cursor_text_append(cur, escape);
}

static inline void
_prepend_escaped_char(Evas_Textblock_Cursor *cur, const char *s,
		     const char *s_end)
{
   const char *escape;

   escape = _escaped_char_get(s, s_end);
   if (escape)
     evas_textblock_cursor_text_prepend(cur, escape);
}

/**
 * to be documented.
 * @param obj  to be documented.
 * @param text to be documented.
 * @return Return no value.
 */
EAPI void
evas_object_textblock_text_markup_set(Evas_Object *obj, const char *text)
{
   TB_HEAD();
   if ((text != o->markup_text) && (o->markup_text))
     {
	free(o->markup_text);
	o->markup_text = NULL;
     }
   _nodes_clear(obj);
   o->formatted.valid = 0;
   o->native.valid = 0;
   o->changed = 1;
   evas_object_change(obj);
   if (!o->style)
     {
	if (text != o->markup_text)
	  {
	     if (text) o->markup_text = strdup(text);
	  }
	return;
     }
   evas_textblock_cursor_node_first(o->cursor);
   if (text)
     {
	char *s, *p;
	char *tag_start, *tag_end, *esc_start, *esc_end;

	tag_start = tag_end = esc_start = esc_end = NULL;
	p = (char *)text;
	s = p;
	for (;;)
	  {
	     if ((*p == 0) ||
		 (tag_end) || (esc_end) ||
		 (tag_start) || (esc_start))
	       {
		  if (tag_end)
		    {
		       char *ttag;
		       size_t ttag_len = tag_end - tag_start -1;

		       ttag = malloc(ttag_len + 1);
		       if (ttag)
			 {
			    const char *match;
			    size_t replace_len;

			    memcpy(ttag, tag_start + 1, ttag_len);
			    ttag[ttag_len] = 0;
			    match = _style_match_tag(o->style, ttag, ttag_len, &replace_len);
			    if (match)
			      evas_textblock_cursor_format_append(o->cursor, match);
			    else
			      {
				 char *ttag2;

				 ttag2 = malloc(ttag_len + 2 + 1);
				 if (ttag2)
				   {
				      if (ttag[0] == '/')
					{
					   strcpy(ttag2, "- ");
					   strcat(ttag2, ttag + 1);
					}
				      else
					{
					   strcpy(ttag2, "+ ");
					   strcat(ttag2, ttag);
					}
				      evas_textblock_cursor_format_append(o->cursor, ttag2);
				      free(ttag2);
				   }
			      }
			    free(ttag);
			 }
		       tag_start = tag_end = NULL;
		    }
		  else if (esc_end)
		    {
		       _append_escaped_char(o->cursor, esc_start, esc_end);
		       esc_start = esc_end = NULL;
		    }
		  else if (*p == 0)
		    {
		       _append_text_run(o, s, p);
		       s = NULL;
		    }
		  if (*p == 0)
		    break;
	       }
	     if (*p == '<')
	       {
		  if (!esc_start)
		    {
		       tag_start = p;
		       tag_end = NULL;
		       _append_text_run(o, s, p);
		       s = NULL;
		    }
	       }
	     else if (*p == '>')
	       {
		  if (tag_start)
		    {
		       tag_end = p;
		       s = p + 1;
		    }
	       }
	     else if (*p == '&')
	       {
		  if (!tag_start)
		    {
		       esc_start = p;
		       esc_end = NULL;
		       _append_text_run(o, s, p);
		       s = NULL;
		    }
	       }
	     else if (*p == ';')
	       {
		  if (esc_start)
		    {
		       esc_end = p;
		       s = p + 1;
		    }
	       }
	     p++;
	  }
     }
     {
	Eina_List *l;
	Evas_Textblock_Cursor *cursor;

	EINA_LIST_FOREACH(o->cursors, l, cursor)
	  evas_textblock_cursor_node_first(cursor);
     }
}

/**
 * to be documented.
 * @param cur  to be documented.
 * @param text to be documented.
 * @return Return no value.
 */
EAPI void
evas_object_textblock_text_markup_prepend(Evas_Textblock_Cursor *cur, const char *text)
{
   Evas_Object_Textblock *o;

   if (!cur) return;
   o = (Evas_Object_Textblock *)(cur->obj->object_data);
   if (o->markup_text)
     {
	free(o->markup_text);
	o->markup_text = NULL;
     }
   o->formatted.valid = 0;
   o->native.valid = 0;
   o->changed = 1;
   evas_object_change(cur->obj);
   if (!o->style) return;
   if (text)
     {
	char *s, *p;
	char *tag_start, *tag_end, *esc_start, *esc_end;

	tag_start = tag_end = esc_start = esc_end = NULL;
	p = (char *)text;
	s = p;
	for (;;)
	  {
	     if ((*p == 0) ||
		 (tag_end) || (esc_end) ||
		 (tag_start) || (esc_start))
	       {
		  if (tag_end)
		    {
		       char *ttag;
		       size_t ttag_len = tag_end - tag_start - 1;

		       ttag = malloc(ttag_len + 1);
		       if (ttag)
			 {
			    const char *match;
			    size_t replace_len;

			    strncpy(ttag, tag_start + 1, ttag_len);
			    ttag[ttag_len] = 0;
			    match = _style_match_tag(o->style, ttag, ttag_len, &replace_len);
			    if (match)
			      evas_textblock_cursor_format_prepend(cur, match);
			    else
			      {
				 char *ttag2;

				 ttag2 = malloc(ttag_len + 2 + 1);
				 if (ttag2)
				   {
				      if (ttag[0] == '/')
					{
					   strcpy(ttag2, "- ");
					   strcat(ttag2, ttag + 1);
					}
				      else
					{
					   strcpy(ttag2, "+ ");
					   strcat(ttag2, ttag);
					}
				      evas_textblock_cursor_format_prepend(o->cursor, ttag2);
				      free(ttag2);
				   }
			      }
			    free(ttag);
			 }
		       tag_start = tag_end = NULL;
		    }
		  else if (esc_end)
		    {
		       _prepend_escaped_char(cur, esc_start, esc_end);
		       esc_start = esc_end = NULL;
		    }
		  else if (*p == 0)
		    {
		       _prepend_text_run(o, s, p);
		       s = NULL;
		    }
		  if (*p == 0)
		    break;
	       }
	     if (*p == '<')
	       {
		  if (!esc_start)
		    {
		       tag_start = p;
		       tag_end = NULL;
		       _prepend_text_run(o, s, p);
		       s = NULL;
		    }
	       }
	     else if (*p == '>')
	       {
		  if (tag_start)
		    {
		       tag_end = p;
		       s = p + 1;
		    }
	       }
	     else if (*p == '&')
	       {
		  if (!tag_start)
		    {
		       esc_start = p;
		       esc_end = NULL;
		       _prepend_text_run(o, s, p);
		       s = NULL;
		    }
	       }
	     else if (*p == ';')
	       {
		  if (esc_start)
		    {
		       esc_end = p;
		       s = p + 1;
		    }
	       }
	     p++;
	  }
     }
}

/**
 * to be documented.
 * @param obj  to be documented.
 * @return to be documented.
 */
EAPI const char *
evas_object_textblock_text_markup_get(const Evas_Object *obj)
{
   Evas_Object_Textblock_Node *n;
   Eina_Strbuf *txt = NULL;

   TB_HEAD_RETURN(NULL);
   if (o->markup_text) return(o->markup_text);
   txt = eina_strbuf_new();
   EINA_INLIST_FOREACH(o->nodes, n)
     {
	size_t replace_len = eina_strbuf_length_get(n->text);
        if ((n->type == NODE_FORMAT) && replace_len)
	  {
	     size_t tag_len;
	     const char *tag = _style_match_replace(o->style, eina_strbuf_string_get(n->text), replace_len, &tag_len);
	     eina_strbuf_append_char(txt, '<');
	     if (tag)
	       {
		  // FIXME: need to escape
		  eina_strbuf_append_length(txt, tag, tag_len);
	       }
	     else
	       {
		  const char *s;
		  int push = 0;
		  int pop = 0;

		  // FIXME: need to escape
		  s = eina_strbuf_string_get(n->text);
		  if (*s == '+') push = 1;
		  if (*s == '-') pop = 1;
		  while ((*s == ' ') || (*s == '+') || (*s == '-')) s++;
		  if (pop) eina_strbuf_append_char(txt, '/');
		  if (push) eina_strbuf_append(txt, "+ ");
		  eina_strbuf_append(txt, s);
	       }
	     eina_strbuf_append_char(txt, '>');
	  }
	else if ((n->type == NODE_TEXT) && eina_strbuf_length_get(n->text))
	  {
	     const char *p = eina_strbuf_string_get(n->text);

	     while (*p)
	       {
		  const char *escape;
		  int adv;

		  escape = _escaped_char_match(p, &adv);
		  if (escape)
		    {
		       p += adv;
		       eina_strbuf_append(txt, escape);
		    }
		  else
		    {
		       eina_strbuf_append_char(txt, *p);
		       p++;
		    }
	       }
	  }
     }
   o->markup_text = eina_strbuf_string_steal(txt);
   eina_strbuf_free(txt);
   return o->markup_text;
}

/* {{{ Cursors */

/* cursors */
/**
 * to be documented.
 * @param obj  to be documented.
 * @return to be documented.
 */
EAPI const Evas_Textblock_Cursor *
evas_object_textblock_cursor_get(const Evas_Object *obj)
{
   TB_HEAD_RETURN(NULL);
   return o->cursor;
}

/**
 * to be documented.
 * @param obj  to be documented.
 * @return to be documented.
 */
EAPI Evas_Textblock_Cursor *
evas_object_textblock_cursor_new(Evas_Object *obj)
{
   Evas_Textblock_Cursor *cur;

   TB_HEAD_RETURN(NULL);
   cur = calloc(1, sizeof(Evas_Textblock_Cursor));
   cur->obj = obj;
   cur->node = o->nodes;
   cur->pos = 0;
   cur->eol = 0;
   o->cursors = eina_list_append(o->cursors, cur);
   return cur;
}

/**
 * to be documented.
 * @param cur  to be documented.
 * @return Returns no value.
 */
EAPI void
evas_textblock_cursor_free(Evas_Textblock_Cursor *cur)
{
   Evas_Object_Textblock *o;

   if (!cur) return;
   o = (Evas_Object_Textblock *)(cur->obj->object_data);
   if (cur == o->cursor) return;
   o->cursors = eina_list_remove(o->cursors, cur);
   free(cur);
}

/**
 * to be documented.
 * @param cur  to be documented.
 * @return Returns no value.
 */
EAPI void
evas_textblock_cursor_node_first(Evas_Textblock_Cursor *cur)
{
   Evas_Object_Textblock *o;

   if (!cur) return;
   o = (Evas_Object_Textblock *)(cur->obj->object_data);
   cur->node = o->nodes;
   cur->pos = 0;
   cur->eol = 0;
}

/**
 * to be documented.
 * @param cur  to be documented.
 * @return Returns no value.
 */
EAPI void
evas_textblock_cursor_node_last(Evas_Textblock_Cursor *cur)
{
   Evas_Object_Textblock *o;

   if (!cur) return;
   o = (Evas_Object_Textblock *)(cur->obj->object_data);
   if (o->nodes)
     {
	cur->node = (Evas_Object_Textblock_Node *)((EINA_INLIST_GET(o->nodes))->last);
	cur->pos = 0;
        cur->eol = 0;  // 1
	evas_textblock_cursor_char_last(cur);
     }
   else
     {
	cur->node = NULL;
	cur->pos = 0;
        cur->eol = 0; // 1
     }
}

/**
 * to be documented.
 * @param cur  to be documented.
 * @return to be documented.
 */
EAPI Eina_Bool
evas_textblock_cursor_node_next(Evas_Textblock_Cursor *cur)
{
   if (!cur) return EINA_FALSE;
   if (!cur->node) return EINA_FALSE;
   if ((EINA_INLIST_GET(cur->node))->next)
     {
	cur->node = (Evas_Object_Textblock_Node *)((EINA_INLIST_GET(cur->node))->next);
	cur->pos = 0;
        cur->eol = 0;
	return EINA_TRUE;
     }
   return EINA_FALSE;
}

/**
 * to be documented.
 * @param cur  to be documented.
 * @return to be documented.
 */
EAPI Eina_Bool
evas_textblock_cursor_node_prev(Evas_Textblock_Cursor *cur)
{
   if (!cur) return EINA_FALSE;
   if (!cur->node) return EINA_FALSE;
   if ((EINA_INLIST_GET(cur->node))->prev)
     {
	cur->node = (Evas_Object_Textblock_Node *)((EINA_INLIST_GET(cur->node))->prev);
	evas_textblock_cursor_char_last(cur);
	return EINA_TRUE;
     }
   return EINA_FALSE;
}

/**
 * to be documented.
 * @param cur  to be documented.
 * @return to be documented.
 */
EAPI Eina_Bool
evas_textblock_cursor_char_next(Evas_Textblock_Cursor *cur)
{
   int index, ch;

   if (!cur) return EINA_FALSE;
   if (!cur->node) return EINA_FALSE;
   if (cur->node->type == NODE_FORMAT) return EINA_FALSE;
   if (!eina_strbuf_length_get(cur->node->text)) return EINA_FALSE;
   index = cur->pos;

   if (cur->node->type == NODE_TEXT)
     {
        Evas_Object_Textblock_Line *ln = NULL;
        Evas_Object_Textblock_Item *it = NULL;
        int pos;

	_find_layout_item_line_match(cur->obj, cur->node, cur->pos, cur->eol, &ln, &it);
        if (it)
          {
             pos = cur->pos - it->source_pos;
             if (pos <= 0) index -= pos;
          }
        else
          printf("TB: 'it' not found\n");
     }

   ch = evas_common_font_utf8_get_next((unsigned char *)eina_strbuf_string_get(cur->node->text), &index);
   if ((ch == 0) || (index < 0)) return EINA_FALSE;
   if (eina_strbuf_string_get(cur->node->text)[index] == 0) return EINA_FALSE;
   cur->pos = index;
   cur->eol = 0; // 1
   return EINA_TRUE;
}

/**
 * to be documented.
 * @param cur  to be documented.
 * @return to be documented.
 */
EAPI Eina_Bool
evas_textblock_cursor_char_prev(Evas_Textblock_Cursor *cur)
{
   int index;
   int at_end_of_line = 0;
   int at_start_of_line = 0;

   if (!cur) return EINA_FALSE;
   if (!cur->node) return EINA_FALSE;
   if (cur->node->type == NODE_FORMAT) return EINA_FALSE;
   if (!eina_strbuf_length_get(cur->node->text)) return EINA_FALSE;
   index = cur->pos;
   if (index == 0) return EINA_FALSE;

   // XXX: FIXME: determine at_end_of_line and at_start_of_line

   if (cur->node->type == NODE_TEXT)
     {
        Evas_Object_Textblock_Line *ln = NULL;
        Evas_Object_Textblock_Item *it = NULL;
        int pos;

	_find_layout_item_line_match(cur->obj, cur->node, cur->pos, cur->eol, &ln, &it);
        if (it)
          {
             pos = cur->pos - it->source_pos;
             if (pos <= 0) at_start_of_line = 1;
             if (it->text)
               {
                  int plast;

                  plast = evas_common_font_utf8_get_last((unsigned char *) it->text, strlen(it->text));
                  if ((index - it->source_pos) == plast) at_end_of_line = 1;
               }
          }
     }

   if ((cur->eol) && (at_end_of_line))
     {
        cur->eol = 0;
        return EINA_TRUE;
     }
   evas_common_font_utf8_get_prev((unsigned char *)eina_strbuf_string_get(cur->node->text), &index);
   if (/*(ch == 0) || */(index < 0)) return EINA_FALSE;
   cur->pos = index;
   if (at_start_of_line)
     cur->eol =1;
   else
     cur->eol = 0;
   return EINA_TRUE;
}

/**
 * to be documented.
 * @param cur  to be documented.
 * @return Returns no value.
 */
EAPI void
evas_textblock_cursor_char_first(Evas_Textblock_Cursor *cur)
{
   if (!cur) return;
   cur->pos = 0;
   cur->eol = 0;
}

/**
 * to be documented.
 * @param cur  to be documented.
 * @return Returns no value.
 */
EAPI void
evas_textblock_cursor_char_last(Evas_Textblock_Cursor *cur)
{
   int index;

   if (!cur) return;
   if (!cur->node) return;
   if (cur->node->type == NODE_FORMAT)
     {
	cur->pos = 0;
	return;
     }
   index = evas_common_font_utf8_get_last((unsigned char *)eina_strbuf_string_get(cur->node->text), eina_strbuf_length_get(cur->node->text));
   if (index < 0) cur->pos = 0;
   cur->pos = index;
   cur->eol = 0; // 1
}

/**
 * to be documented.
 * @param cur  to be documented.
 * @return Returns no value.
 */
EAPI void
evas_textblock_cursor_line_first(Evas_Textblock_Cursor *cur)
{
   Evas_Object_Textblock *o;
   Evas_Object_Textblock_Line *ln = NULL;
   Evas_Object_Textblock_Item *it = NULL;
   Evas_Object_Textblock_Format_Item *fi = NULL;

   if (!cur) return;
   if (!cur->node) return;
   o = (Evas_Object_Textblock *)(cur->obj->object_data);
   if (!o->formatted.valid) _relayout(cur->obj);
   if (cur->node->type == NODE_FORMAT)
     _find_layout_format_item_line_match(cur->obj, cur->node, &ln, &fi);
   else
     _find_layout_item_line_match(cur->obj, cur->node, cur->pos, cur->eol, &ln, &it);
   if (!ln) return;
   cur->eol = 0;
   it = (Evas_Object_Textblock_Item *)ln->items;
   fi = (Evas_Object_Textblock_Format_Item *)ln->format_items;
   if ((it) && (fi))
     {
	if (it->x < fi->x) fi = NULL;
	else it = NULL;
     }
   if (it)
     {
	cur->pos = it->source_pos;
	cur->node = it->source_node;
     }
   else if (fi)
     {
	cur->pos = 0;
	cur->node = fi->source_node;
     }
}

/**
 * to be documented.
 * @param cur  to be documented.
 * @return Returns no value.
 */
EAPI void
evas_textblock_cursor_line_last(Evas_Textblock_Cursor *cur)
{
   Evas_Object_Textblock *o;
   Evas_Object_Textblock_Line *ln = NULL;
   Evas_Object_Textblock_Item *it = NULL;
   Evas_Object_Textblock_Format_Item *fi = NULL;

   if (!cur) return;
   if (!cur->node) return;
   o = (Evas_Object_Textblock *)(cur->obj->object_data);
   if (!o->formatted.valid) _relayout(cur->obj);
// kills "click below text" and up/downm arrow. disable   
//   cur->eol = 1;
   if (cur->node->type == NODE_FORMAT)
     _find_layout_format_item_line_match(cur->obj, cur->node, &ln, &fi);
   else
     _find_layout_item_line_match(cur->obj, cur->node, cur->pos, cur->eol, &ln, &it);
   if (!ln) return;
   if (ln->items)
     it = (Evas_Object_Textblock_Item *)((EINA_INLIST_GET(ln->items))->last);
   else
     it = NULL;
   if (ln->format_items)
     fi = (Evas_Object_Textblock_Format_Item *)((EINA_INLIST_GET(ln->format_items))->last);
   else
     fi = NULL;
   if ((it) && (fi))
     {
	if ((it->x + it->w) > (fi->x + fi->w)) fi = NULL;
	else it = NULL;
     }
   if (it)
     {
	int index;

	cur->pos = it->source_pos;
	cur->node = it->source_node;
	index = evas_common_font_utf8_get_last((unsigned char *)it->text, strlen(it->text));
        if ((index >= 0) && (it->text[0] != 0))
          evas_common_font_utf8_get_next((unsigned char *)(it->text), &index);
	if (index >= 0) cur->pos += index;
     }
   else if (fi)
     {
	cur->pos = 0;
        cur->eol = 0;
	cur->node = fi->source_node;
     }
}

/**
 * to be documented.
 * @param cur  to be documented.
 * @return to be documented.
 */
EAPI int
evas_textblock_cursor_pos_get(const Evas_Textblock_Cursor *cur)
{
   if (!cur) return -1;
   return cur->pos;
}

/**
 * to be documented.
 * @param cur to be documented.
 * @param pos to be documented.
 */
EAPI void
evas_textblock_cursor_pos_set(Evas_Textblock_Cursor *cur, int pos)
{
   unsigned int len;

   if (!cur) return;
   if (!cur->node) return;
   if (cur->node->type == NODE_FORMAT) pos = 0;
   len = eina_strbuf_length_get(cur->node->text);
   if (pos < 0) pos = 0;
   else if (pos > len) pos = len;
   cur->pos = pos;
   cur->eol = 0;
}

/**
 * to be documented.
 * @param cur to be documented.
 * @param line to be documented.
 * @return to be documented.
 */
EAPI Eina_Bool
evas_textblock_cursor_line_set(Evas_Textblock_Cursor *cur, int line)
{
   Evas_Object_Textblock *o;
   Evas_Object_Textblock_Line *ln;
   Evas_Object_Textblock_Item *it;
   Evas_Object_Textblock_Format_Item *fi;

   if (!cur) return EINA_FALSE;
   o = (Evas_Object_Textblock *)(cur->obj->object_data);
   if (!o->formatted.valid) _relayout(cur->obj);

   ln = _find_layout_line_num(cur->obj, line);
   if (!ln) return EINA_FALSE;
   it = (Evas_Object_Textblock_Item *)ln->items;
   fi = (Evas_Object_Textblock_Format_Item *)ln->format_items;
   if ((it) && (fi))
     {
	if (it->x < fi->x) fi = NULL;
	else it = NULL;
     }
   if (it)
     {
	cur->pos = it->source_pos;
        cur->eol = 0;
	cur->node = it->source_node;
     }
   else if (fi)
     {
	cur->pos = 0;
        cur->eol = 0;
	cur->node = fi->source_node;
     }
   else
     {
        cur->pos = 0;
        cur->eol = 0;
        cur->node = o->nodes;
     }
   return EINA_TRUE;
}

/**
 * to be documented.
 * @param cur1 to be documented.
 * @param cur2 to be documented.
 * @return to be documented.
 */
EAPI int
evas_textblock_cursor_compare(const Evas_Textblock_Cursor *cur1, const Evas_Textblock_Cursor *cur2)
{
   Eina_Inlist *l1, *l2;

   if (!cur1) return 0;
   if (!cur2) return 0;
   if (cur1->obj != cur2->obj) return 0;
   if ((!cur1->node) || (!cur2->node)) return 0;
   if (cur1->node == cur2->node)
     {
	if (cur1->pos < cur2->pos) return -1; /* cur1 < cur2 */
	else if (cur1->pos > cur2->pos) return 1; /* cur2 < cur1 */
        if ((cur1->eol) == (cur1->eol)) return 0; /* cur1 == cur2 */
        if (cur1->eol) return 1; /* cur2 < cur1 */
	return -1; /* cur1 < cur2 */
     }
   for (l1 = EINA_INLIST_GET(cur1->node),
        l2 = EINA_INLIST_GET(cur1->node); (l1) || (l2);)
     {
	if (l1 == EINA_INLIST_GET(cur2->node)) return 1; /* cur2 < cur 1 */
	else if (l2 == EINA_INLIST_GET(cur2->node)) return -1; /* cur1 < cur 2 */
	else if (!l1) return -1; /* cur1 < cur 2 */
	else if (!l2) return 1; /* cur2 < cur 1 */
	if (l1) l1 = l1->prev;
	if (l2) l2 = l2->next;
     }
   return 0;
}

/**
 * to be documented.
 * @param cur to be documented.
 * @param cur_dest to be documented.
 * @return Returns no value.
 */
EAPI void
evas_textblock_cursor_copy(const Evas_Textblock_Cursor *cur, Evas_Textblock_Cursor *cur_dest)
{
   if (!cur) return;
   if (!cur_dest) return;
   if (cur->obj != cur_dest->obj) return;
   cur_dest->pos = cur->pos;
   cur_dest->node = cur->node;
   cur_dest->eol = cur->eol;
}


/* text controls */
/**
 * to be documented.
 * @param cur to be documented.
 * @param text to be documented.
 * @return Returns no value.
 */
EAPI void
evas_textblock_cursor_text_append(Evas_Textblock_Cursor *cur, const char *text)
{
   Evas_Object_Textblock *o;
   Evas_Object_Textblock_Node *n, *nrel;
   int index, ch;

   if (!cur) return;

   /* Adjust cursors pointing to the same Node */
   o = (Evas_Object_Textblock *)(cur->obj->object_data);
   if (text)
     {
	Eina_List *l;
	Evas_Textblock_Cursor *data;

	EINA_LIST_FOREACH(o->cursors, l, data)
	  {
	     if (data != cur)
	       {
		  if (cur->node == data->node)
		    {
		       if (data->pos > cur->pos)
                         {
                            data->pos += strlen(text);
                         }
		    }
	       }
	  }
     }

   /*
    * If current node is nonexisting (aka cur points to EOF) or FORMAT one, make
    * a text node and insert it in right place
    */
   n = cur->node;
   if ((!n) || (n->type == NODE_FORMAT))
     {
	nrel = n;
	n = calloc(1, sizeof(Evas_Object_Textblock_Node));
	n->type = NODE_TEXT;
	n->text = eina_strbuf_new();
	if (nrel)
	  o->nodes = (Evas_Object_Textblock_Node *)eina_inlist_append_relative(EINA_INLIST_GET(o->nodes),
									       EINA_INLIST_GET(n),
									       EINA_INLIST_GET(nrel));
	else
	  o->nodes = (Evas_Object_Textblock_Node *)eina_inlist_append(EINA_INLIST_GET(o->nodes), EINA_INLIST_GET(n));

        /*
         * Adjust cur to point to newly created node
         */
        cur->node = n;
     }

   /* Single-character advance?! */
   index = cur->pos;
   if (eina_strbuf_length_get(n->text))
     {
	ch = evas_common_font_utf8_get_next((unsigned char *)eina_strbuf_string_get(n->text), &index);
	if (ch != 0)
          {
             cur->pos = index;
          }
     }
   if (cur->pos >= (eina_strbuf_length_get(n->text) - 1))
     eina_strbuf_append(n->text, (char *)text);
   else
     eina_strbuf_insert(n->text, (char *)text, cur->pos);
// XXX: This makes no sense?
   if (text)
     {
        cur->pos += strlen(text);
     }
   o->formatted.valid = 0;
   o->native.valid = 0;
   o->changed = 1;
   if (o->markup_text)
     {
	free(o->markup_text);
	o->markup_text = NULL;
     }
   _nodes_adjacent_merge(cur->obj, n);
   evas_object_change(cur->obj);
}

/**
 * to be documented.
 * @param cur to be documented.
 * @param text to be documented.
 * @return Returns no value.
 */
EAPI void
evas_textblock_cursor_text_prepend(Evas_Textblock_Cursor *cur, const char *text)
{
   Evas_Object_Textblock *o;
   Evas_Object_Textblock_Node *n, *nrel;

   if (!cur) return;

   /* Adjust cursors pointing to the same Node */
   o = (Evas_Object_Textblock *)(cur->obj->object_data);
     {
	Eina_List *l;
	Evas_Textblock_Cursor *data;

	EINA_LIST_FOREACH(o->cursors, l, data)
	  {
	     if (data != cur)
	       {
		  if (cur->node == data->node)
		    {
                        /* FIXME: Why not in _append? */
		       if (data->node &&
			   (data->node->type == NODE_TEXT) &&
			   (data->pos >= cur->pos))
                         {
                            data->pos += strlen(text);
                         }
		    }
	       }
	  }
     }

   /*
    * If current node is nonexisting (aka cur points to EOF) or FORMAT one, make
    * a text node and insert it in right place
    */
   n = cur->node;
   if ((!n) || (n->type == NODE_FORMAT))
     {
	nrel = n;
	n = calloc(1, sizeof(Evas_Object_Textblock_Node));
	n->type = NODE_TEXT;
	n->text = eina_strbuf_new();
	if (nrel)
	  o->nodes = (Evas_Object_Textblock_Node *)eina_inlist_prepend_relative(EINA_INLIST_GET(o->nodes),
										EINA_INLIST_GET(n),
										EINA_INLIST_GET(nrel));
	else
	  o->nodes = (Evas_Object_Textblock_Node *)eina_inlist_prepend(EINA_INLIST_GET(o->nodes), EINA_INLIST_GET(n));

        /*
         * Adjust cur to point to newly created node
         */
        cur->node = n;
     }
   if (!n->text) n->text = eina_strbuf_new();
   
   if (text)
     {
        if (cur->pos > (eina_strbuf_length_get(n->text) - 1))
          {
             eina_strbuf_append(n->text, (char *)text);
          }
        else
          {
             eina_strbuf_insert(n->text, (char *)text, cur->pos);
          }
        cur->pos += strlen(text);
     }
   o->formatted.valid = 0;
   o->native.valid = 0;
   o->changed = 1;
   if (o->markup_text)
     {
	free(o->markup_text);
	o->markup_text = NULL;
     }
   _nodes_adjacent_merge(cur->obj, n);
   evas_object_change(cur->obj);
}

/**
 * to be documented.
 * @param cur to be documented.
 * @param format to be documented.
 * @return Returns no value.
 */
EAPI void
evas_textblock_cursor_format_append(Evas_Textblock_Cursor *cur, const char *format)
{
   Evas_Object_Textblock *o;
   Evas_Object_Textblock_Node *n, *nc, *n2;

   if (!cur) return;
   if ((!format) || (format[0] == 0)) return;
   o = (Evas_Object_Textblock *)(cur->obj->object_data);
   nc = cur->node;
   n = calloc(1, sizeof(Evas_Object_Textblock_Node));
   n->type = NODE_FORMAT;
   n->text = eina_strbuf_new();
   eina_strbuf_append(n->text, format);
   if (!nc)
     {
        o->nodes = (Evas_Object_Textblock_Node *)eina_inlist_append(EINA_INLIST_GET(o->nodes), EINA_INLIST_GET(n));
     }
   else if (nc->type == NODE_FORMAT)
     {
        o->nodes = (Evas_Object_Textblock_Node *)eina_inlist_append_relative(EINA_INLIST_GET(o->nodes),
									     EINA_INLIST_GET(n),
									     EINA_INLIST_GET(nc));
     }
   else if (nc->type == NODE_TEXT)
     {
	int index, ch = 0;

	index = cur->pos;
	if (eina_strbuf_length_get(nc->text))
	  {
	     ch = evas_common_font_utf8_get_next((unsigned char *)eina_strbuf_string_get(nc->text), &index);
	     if (ch != 0)
	       cur->pos = index;
	  }
	o->nodes = (Evas_Object_Textblock_Node *)eina_inlist_append_relative(EINA_INLIST_GET(o->nodes),
									     EINA_INLIST_GET(n),
									     EINA_INLIST_GET(nc));
	if ((ch != 0) && (cur->pos < eina_strbuf_length_get(nc->text)))
	  {
	     n2 = calloc(1, sizeof(Evas_Object_Textblock_Node));
	     n2->type = NODE_TEXT;
	     n2->text = eina_strbuf_new();
	     eina_strbuf_append(n2->text, (eina_strbuf_string_get(nc->text) + cur->pos));
	     o->nodes = (Evas_Object_Textblock_Node *)eina_inlist_append_relative(EINA_INLIST_GET(o->nodes),
										  EINA_INLIST_GET(n2),
										  EINA_INLIST_GET(n));

	     eina_strbuf_remove(nc->text, cur->pos, eina_strbuf_length_get(nc->text));
	  }
     }
   cur->node = n;
// XXX: This makes no sense
   cur->pos = 0;
   o->formatted.valid = 0;
   o->native.valid = 0;
   o->changed = 1;
   if (o->markup_text)
     {
	free(o->markup_text);
	o->markup_text = NULL;
     }
   evas_object_change(cur->obj);
}

/**
 * to be documented.
 * @param cur to be documented.
 * @param format to be documented.
 * @return Returns no value.
 */
EAPI void
evas_textblock_cursor_format_prepend(Evas_Textblock_Cursor *cur, const char *format)
{
   Evas_Object_Textblock *o;
   Evas_Object_Textblock_Node *n, *nc, *n2;

   if (!cur) return;
   if ((!format) || (format[0] == 0)) return;
   o = (Evas_Object_Textblock *)(cur->obj->object_data);
   nc = cur->node;
   n = calloc(1, sizeof(Evas_Object_Textblock_Node));
   n->type = NODE_FORMAT;
   n->text = eina_strbuf_new();
   eina_strbuf_append(n->text, format);
   if (!nc)
     {
	o->nodes = (Evas_Object_Textblock_Node *)eina_inlist_prepend(EINA_INLIST_GET(o->nodes), EINA_INLIST_GET(n));
	cur->node = n;
	cur->pos = 0;
     }
   else if (nc->type == NODE_FORMAT)
     {
        o->nodes = (Evas_Object_Textblock_Node *)eina_inlist_prepend_relative(EINA_INLIST_GET(o->nodes),
									      EINA_INLIST_GET(n),
									      EINA_INLIST_GET(nc));
	cur->node = nc;
	cur->pos = 0;
     }
   else if (nc->type == NODE_TEXT)
     {
        int len;

        len = eina_strbuf_length_get(nc->text);
	if (cur->pos == 0)
	  o->nodes = (Evas_Object_Textblock_Node *)eina_inlist_prepend_relative(EINA_INLIST_GET(o->nodes),
										EINA_INLIST_GET(n),
										EINA_INLIST_GET(nc));
	else
	  o->nodes = (Evas_Object_Textblock_Node *)eina_inlist_append_relative(EINA_INLIST_GET(o->nodes),
									       EINA_INLIST_GET(n),
									       EINA_INLIST_GET(nc));
	if ((cur->pos < len) && (cur->pos != 0))
	  {
	     n2 = calloc(1, sizeof(Evas_Object_Textblock_Node));
	     n2->type = NODE_TEXT;
	     n2->text = eina_strbuf_new();
	     eina_strbuf_append(n2->text, 
                                (eina_strbuf_string_get(nc->text) + cur->pos));
	     o->nodes = (Evas_Object_Textblock_Node *)eina_inlist_append_relative(EINA_INLIST_GET(o->nodes),
										  EINA_INLIST_GET(n2),
										  EINA_INLIST_GET(n));
	     eina_strbuf_remove(nc->text, cur->pos, eina_strbuf_length_get(nc->text));
	     cur->node = n2;
	     cur->pos = 0;
//             cur->eol = 0;
	  }
	else if (cur->pos == len)
	  {
             if (EINA_INLIST_GET(n)->next)
               cur->node = EINA_INLIST_GET(n)->next;
             else
               cur->node = n;
             cur->pos = 0;
//             cur->eol = 0;
	  }
	else
	  {
	     cur->node = nc;
	     cur->pos = 0;
//             cur->eol = 0;
	  }
     }
   o->formatted.valid = 0;
   o->native.valid = 0;
   o->changed = 1;
   if (o->markup_text)
     {
	free(o->markup_text);
	o->markup_text = NULL;
     }
   evas_object_change(cur->obj);
}

/**
 * to be documented.
 * @param cur to be documented.
 * @return Returns no value.
 */
EAPI void
evas_textblock_cursor_node_delete(Evas_Textblock_Cursor *cur)
{
   Evas_Object_Textblock *o;
   Evas_Object_Textblock_Node *n, *n2;

   if (!cur) return;
   o = (Evas_Object_Textblock *)(cur->obj->object_data);
   n = cur->node;
   if (eina_strbuf_length_get(n->text) && (!strcmp(eina_strbuf_string_get(n->text), "\n")) &&
       (!(EINA_INLIST_GET(n))->next)) return;
   n2 = (Evas_Object_Textblock_Node *)((EINA_INLIST_GET(n))->next);
   if (n2)
     {
	cur->node = n2;
	cur->pos = 0;
     }
   else
     {
	n2 = (Evas_Object_Textblock_Node *)((EINA_INLIST_GET(n))->prev);
	cur->node = n2;
	cur->pos = 0;
	evas_textblock_cursor_char_last(cur);
     }

     {
	Eina_List *l;
	Evas_Textblock_Cursor *data;

	EINA_LIST_FOREACH(o->cursors, l, data)
	  {
	     if (data != cur)
	       {
		  if (n == data->node)
		    {
		       data->node = cur->node;
		       data->pos = cur->pos;
                       data->eol = cur->eol;
		    }
	       }
	  }
     }

   o->nodes = (Evas_Object_Textblock_Node *)eina_inlist_remove(EINA_INLIST_GET(o->nodes), EINA_INLIST_GET(n));
   if (n->text) eina_strbuf_free(n->text);
   free(n);

   if (n2) _nodes_adjacent_merge(cur->obj, n2);

   o->formatted.valid = 0;
   o->native.valid = 0;
   o->changed = 1;
   if (o->markup_text)
     {
	free(o->markup_text);
	o->markup_text = NULL;
     }
   evas_object_change(cur->obj);
}

/**
 * to be documented.
 * @param cur to be documented.
 * @return Returns no value.
 */
EAPI void
evas_textblock_cursor_char_delete(Evas_Textblock_Cursor *cur)
{
   Evas_Object_Textblock *o;
   Evas_Object_Textblock_Node *n, *n2;
   int chr, index, ppos;

   if (!cur) return;
   o = (Evas_Object_Textblock *)(cur->obj->object_data);
   n = cur->node;
   if (n->type == NODE_FORMAT)
     {
	evas_textblock_cursor_node_delete(cur);
	return;
     }
   index = cur->pos;
   chr = evas_common_font_utf8_get_next((unsigned char *)eina_strbuf_string_get(n->text), &index);
   if (chr == 0) return;
   ppos = cur->pos;
   eina_strbuf_remove(n->text, cur->pos, index);
   if (!eina_strbuf_length_get(n->text))
     {
	evas_textblock_cursor_node_delete(cur);
	return;
     }
   if (cur->pos == eina_strbuf_length_get(n->text))
     {
	n2 = (Evas_Object_Textblock_Node *)((EINA_INLIST_GET(n))->next);
	if (n2)
	  {
	     cur->node = n2;
	     cur->pos = 0;
	  }
	else
	  {
	     cur->pos = 0;
	     evas_textblock_cursor_char_last(cur);
	  }
     }

     {
	Eina_List *l;
	Evas_Textblock_Cursor *data;

	EINA_LIST_FOREACH(o->cursors, l, data)
	  {
	     if (data != cur)
	       {
		  if ((n == data->node) &&
		      (data->pos > ppos))
		    {
		       data->pos -= (index - ppos);
		    }
	       }
	  }
     }

   o->formatted.valid = 0;
   o->native.valid = 0;
   o->changed = 1;
   if (o->markup_text)
     {
	free(o->markup_text);
	o->markup_text = NULL;
     }
   evas_object_change(cur->obj);
}

/**
 * to be documented.
 * @param cur1 to be documented.
 * @param cur2 to be documented.
 * @return Returns no value.
 */
EAPI void
evas_textblock_cursor_range_delete(Evas_Textblock_Cursor *cur1, Evas_Textblock_Cursor *cur2)
{
   Evas_Object_Textblock *o;
   Evas_Object_Textblock_Node *n1, *n2, *n, *tn;
   int chr, index;

   if (!cur1) return;
   if (!cur2) return;
   if (cur1->obj != cur2->obj) return;
   o = (Evas_Object_Textblock *)(cur1->obj->object_data);
   if (evas_textblock_cursor_compare(cur1, cur2) > 0)
     {
	Evas_Textblock_Cursor *tc;

	tc = cur1;
	cur1 = cur2;
	cur2 = tc;
     }
   n1 = cur1->node;
   n2 = cur2->node;
   if ((!n1) || (!n2)) return;
   index = cur2->pos;
   chr = evas_common_font_utf8_get_next((unsigned char *)eina_strbuf_string_get(n2->text), &index);
// XXX: why was this added? this stops sel to end and 
//   if (chr == 0) return;
   if (n1 == n2)
     {
        if (n1->type == NODE_TEXT)
	  {
	    if (cur1->pos == cur2->pos)
	       {
		  evas_textblock_cursor_char_delete(cur1);
		  evas_textblock_cursor_copy(cur1, cur2);
		  return;
	       }
	     eina_strbuf_remove(n1->text, cur1->pos, index);
	     if (!eina_strbuf_length_get(n1->text))
	       {
		  evas_textblock_cursor_node_delete(cur1);
		  evas_textblock_cursor_copy(cur1, cur2);
		  return;
	       }
	     if (cur1->pos >= eina_strbuf_length_get(n1->text))
	       {
		  n2 = (Evas_Object_Textblock_Node *)((EINA_INLIST_GET(n1))->next);
		  if (n2)
		    {
		       cur1->node = n2;
		       cur1->pos = 0;
		    }
		  else
		    {
		       cur1->pos = 0;
		       evas_textblock_cursor_char_last(cur1);
		    }
	       }
	  }
	else
	  evas_textblock_cursor_node_delete(cur1);
	evas_textblock_cursor_copy(cur1, cur2);
     }
   else
     {
	Eina_List *removes, *format_hump = NULL;
	Evas_Textblock_Cursor tcur;
	Eina_Inlist *l;

	tcur.node = n2;
	tcur.pos = 0;
	index = cur2->pos;
	chr = evas_common_font_utf8_get_next((unsigned char *)eina_strbuf_string_get(n2->text), &index);
	if ((chr == 0) || (index >= eina_strbuf_length_get(n2->text)))
	  {
	     tcur.node = (Evas_Object_Textblock_Node *)(EINA_INLIST_GET(n2))->next;
	     tcur.pos = 0;
	     if (!tcur.node)
	       {
		  if (cur1->pos != 0)
		    {
		       tcur.node = n1;
		       index = cur1->pos;
		       chr = evas_common_font_utf8_get_prev((unsigned char *)eina_strbuf_string_get(n2->text), &index);
		       tcur.pos = index;
		    }
		  else
		    {
		       tcur.node = (Evas_Object_Textblock_Node *)(EINA_INLIST_GET(n1))->prev;
		       if ((tcur.node) && (tcur.node->type == NODE_TEXT))
			 tcur.pos = evas_common_font_utf8_get_last((unsigned char *)eina_strbuf_string_get(tcur.node->text), eina_strbuf_length_get(tcur.node->text));
		       else
			 tcur.pos = 0;
		    }
	       }
	  }
	eina_strbuf_remove(n1->text, cur1->pos, eina_strbuf_length_get(n1->text));
	removes = NULL;
	for (l = (EINA_INLIST_GET(n1))->next; l != EINA_INLIST_GET(n2); l = l->next)
	  removes = eina_list_append(removes, l);
	format_hump = NULL;
	if (n1->type == NODE_TEXT)
	  {
	     if (!eina_strbuf_length_get(n1->text))
	       evas_textblock_cursor_node_delete(cur1);
	  }
	else
	  {
	     if (eina_strbuf_length_get(n1->text) && (eina_strbuf_string_get(n1->text)[0] == '+'))
	       format_hump = eina_list_append(format_hump, n1);
	     else
	       {
                  o->nodes = (Evas_Object_Textblock_Node *)eina_inlist_remove(EINA_INLIST_GET(o->nodes), EINA_INLIST_GET(n1));
		  if (n1->text) eina_strbuf_free(n1->text);
		  free(n1);
	       }
	  }
	while (removes)
	  {
	     n = removes->data;
	     if (n->type == NODE_TEXT)
	       {
		  o->nodes = (Evas_Object_Textblock_Node *)eina_inlist_remove(EINA_INLIST_GET(o->nodes), EINA_INLIST_GET(n));
		  if (n->text) eina_strbuf_free(n->text);
		  free(n);
	       }
	     else
	       {
		  if (eina_strbuf_string_get(n->text)[0] == '+')
		    {
		       format_hump = eina_list_append(format_hump, n);
		    }
		  else if (eina_strbuf_string_get(n->text)[0] == '-')
		    {
		       tn = eina_list_data_get(eina_list_last(format_hump));
		       if (tn)
			 {
			    format_hump = eina_list_remove_list(format_hump, eina_list_last(format_hump));
			    o->nodes = (Evas_Object_Textblock_Node *)eina_inlist_remove(EINA_INLIST_GET(o->nodes),
											EINA_INLIST_GET(tn));
			    if (tn->text) eina_strbuf_free(tn->text);
			    free(tn);
			    o->nodes = (Evas_Object_Textblock_Node *)eina_inlist_remove(EINA_INLIST_GET(o->nodes),
											EINA_INLIST_GET(n));
			    if (n->text) eina_strbuf_free(n->text);
			    free(n);
			 }
		    }
		  else
		    {
		       o->nodes = (Evas_Object_Textblock_Node *)eina_inlist_remove(EINA_INLIST_GET(o->nodes),
										   EINA_INLIST_GET(n));
		       if (n->text) eina_strbuf_free(n->text);
		       free(n);
		    }
	       }
	     removes = eina_list_remove_list(removes, removes);
	  }
        if (n2->type == NODE_TEXT)
	  {
	     eina_strbuf_remove(n2->text, 0, index);
	     if (!eina_strbuf_length_get(n2->text))
	       evas_textblock_cursor_node_delete(cur2);
	  }
	else
	  {
	     if (tcur.node == n2)
	       {
		  if ((EINA_INLIST_GET(n2))->next)
		    {
		       tcur.node = (Evas_Object_Textblock_Node *) (EINA_INLIST_GET(n2))->next;
		       tcur.pos = 0;
		    }
		  else
		    {
		       tcur.node = (Evas_Object_Textblock_Node *) (EINA_INLIST_GET(n2))->next;
		       if (tcur.node)
			 {
			    if (tcur.node->type == NODE_TEXT)
			      tcur.pos = evas_common_font_utf8_get_last((unsigned char *)eina_strbuf_string_get(tcur.node->text), eina_strbuf_length_get(tcur.node->text));
			    else
			      tcur.pos = 0;
			 }
		    }
	       }
	     if (eina_strbuf_string_get(n2->text)[0] == '-')
	       {
		  o->nodes = (Evas_Object_Textblock_Node *)eina_inlist_remove(EINA_INLIST_GET(o->nodes),
									      EINA_INLIST_GET(n2));
		  if (n2->text) eina_strbuf_free(n2->text);
		  free(n2);
		  n = eina_list_data_get(eina_list_last(format_hump));
		  if (n)
		    {
		       if (tcur.node == n)
			 {
			    if ((EINA_INLIST_GET(n))->next)
			      {
				 tcur.node = (Evas_Object_Textblock_Node *) (EINA_INLIST_GET(n))->next;
				 tcur.pos = 0;
			      }
			    else
			      {
				 tcur.node = (Evas_Object_Textblock_Node *) (EINA_INLIST_GET(n))->next;
				 if (tcur.node)
				   {
				      if (tcur.node->type == NODE_TEXT)
					tcur.pos = evas_common_font_utf8_get_last((unsigned char *)eina_strbuf_string_get(tcur.node->text), eina_strbuf_length_get(tcur.node->text));
				      else
					tcur.pos = 0;
				   }
			      }
			 }
		       o->nodes = (Evas_Object_Textblock_Node *)eina_inlist_remove(EINA_INLIST_GET(o->nodes),
										   EINA_INLIST_GET(n));
		       if (n->text) eina_strbuf_free(n->text);
		       free(n);
		    }
	       }
	     else
	       {
		  o->nodes = (Evas_Object_Textblock_Node *)eina_inlist_remove(EINA_INLIST_GET(o->nodes),
									      EINA_INLIST_GET(n2));
		  if (n2->text) eina_strbuf_free(n2->text);
		  free(n2);
	       }
	  }
	if (format_hump) eina_list_free(format_hump);
	cur1->node = tcur.node;
	cur1->pos = tcur.pos;
	cur2->node = tcur.node;
	cur2->pos = tcur.pos;
     }

   /* FIXME: adjust cursors that are affected by the change */
   /* this is temporary just avoiding segv's - it sets all other cursors to
    * the same pos as cur1 and cur2
    */
     {
	Eina_List *l;
	Evas_Textblock_Cursor *data;

	EINA_LIST_FOREACH(o->cursors, l, data)
	  {
	     if ((data != cur1) && (data != cur2))
	       {
		  evas_textblock_cursor_copy(cur1, data);
	       }
	  }
     }
   if (cur1->node) _nodes_adjacent_merge(cur1->obj, cur1->node);
   if (cur2->node) _nodes_adjacent_merge(cur2->obj, cur2->node);

   o->formatted.valid = 0;
   o->native.valid = 0;
   o->changed = 1;
   if (o->markup_text)
     {
	free(o->markup_text);
	o->markup_text = NULL;
     }
   evas_object_change(cur1->obj);
}

/**
 * to be documented.
 * @param cur to be documented.
 * @return to be documented.
 */
EAPI const char *
evas_textblock_cursor_node_text_get(const Evas_Textblock_Cursor *cur)
{
   if (!cur) return NULL;
   if (!cur->node) return NULL;
   if (cur->node->type == NODE_TEXT)
     {
	return eina_strbuf_string_get(cur->node->text);
     }
   return NULL;
}

/**
 * to be documented.
 * @param cur to be documented.
 * @return to be documented.
 */
EAPI int
evas_textblock_cursor_node_text_length_get(const Evas_Textblock_Cursor *cur)
{
   if (!cur) return 0;
   if (!cur->node) return 0;
   if (cur->node->type == NODE_TEXT)
     {
	return eina_strbuf_length_get(cur->node->text);
     }
   return 0;
}

/**
 * to be documented.
 * @param cur to be documented.
 * @return to be documented.
 */
EAPI const char *
evas_textblock_cursor_node_format_get(const Evas_Textblock_Cursor *cur)
{
   if (!cur) return NULL;
   if (!cur->node) return NULL;
   if (cur->node->type == NODE_FORMAT)
     {
	return eina_strbuf_string_get(cur->node->text);
     }
   return NULL;
}

/**
 * to be documented.
 * @param cur to be documented.
 * @return to be documented.
 */
EAPI Eina_Bool
evas_textblock_cursor_node_format_is_visible_get(const Evas_Textblock_Cursor *cur)
{
   Evas_Object_Textblock_Node *n;

   if (!cur) return EINA_FALSE;
   n = cur->node;
   if (!n) return EINA_FALSE;
   if (n->type != NODE_FORMAT) return EINA_FALSE;
   if (!eina_strbuf_length_get(n->text)) return EINA_FALSE;
     {
	char *s;
	char *item;
	int visible = 0;

	s = (char *)eina_strbuf_string_get(n->text);
	if (s[0] == '+' || s[0] == '-')
	  {
	     s++;
	  }
	while ((item = _format_parse(&s)))
	  {
	     char tmp_delim = *s;
	     *s = '\0';
	     if ((!strcmp(item, "\n")) || (!strcmp(item, "\\n")))
	       visible = 1;
	     else if ((!strcmp(item, "\t")) || (!strcmp(item, "\\t")))
	       visible = 1;
	     *s = tmp_delim;
	     if (visible) return EINA_TRUE;
	  }
     }
   return EINA_FALSE;
}

/**
 * to be documented.
 * @param cur1 to be documented.
 * @param cur2 to be documented.
 * @param format to be documented.
 * @return to be documented.
 */
EAPI char *
evas_textblock_cursor_range_text_get(const Evas_Textblock_Cursor *cur1, const Evas_Textblock_Cursor *cur2, Evas_Textblock_Text_Type format)
{
   Evas_Object_Textblock *o;
   Evas_Object_Textblock_Node *n1, *n2, *n;
   Eina_Strbuf *txt;
   char *s, *ret;
   int index;

   if (!cur1) return NULL;
   if (!cur2) return NULL;
   if (cur1->obj != cur2->obj) return NULL;
   o = (Evas_Object_Textblock *)(cur1->obj->object_data);
   if (evas_textblock_cursor_compare(cur1, cur2) > 0)
     {
	const Evas_Textblock_Cursor *tc;

	tc = cur1;
	cur1 = cur2;
	cur2 = tc;
     }
   n1 = cur1->node;
   n2 = cur2->node;
   index = cur2->pos;
   if ((!n1) || (!n2)) return NULL;
   if (n2->text)
     evas_common_font_utf8_get_next((unsigned char *)eina_strbuf_string_get(n2->text), &index);
   txt = eina_strbuf_new();
   EINA_INLIST_FOREACH(n1, n)
     {
	if ((n->type == NODE_TEXT) && (n->text))
	  {
	     s = (char *)eina_strbuf_string_get(n->text);
	     if (format == EVAS_TEXTBLOCK_TEXT_MARKUP)
	       {
		  const char *p, *ps, *pe;

		  if (eina_strbuf_length_get(n->text))
		    {
		       if ((n == n1) && (n == n2))
			 {
			    ps = eina_strbuf_string_get(n->text) + cur1->pos;
			    pe = ps + index - cur1->pos;
			 }
		       else if (n == n1)
			 {
			    ps = eina_strbuf_string_get(n->text) + cur1->pos;
			    pe = ps + strlen(ps);
			 }
		       else if (n == n2)
			 {
			    ps = eina_strbuf_string_get(n->text);
			    pe = ps + cur2->pos + 1;
			 }
		       else
			 {
			    ps = eina_strbuf_string_get(n->text);
			    pe = ps + strlen(ps);
			 }
		       p = ps;
		       while (p < pe)
			 {
			    const char *escape;
			    int adv;

                            if (!*p) break;
			    escape = _escaped_char_match(p, &adv);
			    if (escape)
			      {
				 p += adv;
				 eina_strbuf_append(txt, escape);
			      }
			    else
			      {
				 eina_strbuf_append_char(txt, *p);
				 p++;
			      }
			 }
		    }
	       }
	     else
	       {
		  if ((n == n1) && (n == n2))
		    {
		       s += cur1->pos;
		       eina_strbuf_append_n(txt, s, index - cur1->pos);
		    }
		  else if (n == n1)
		    {
		       s += cur1->pos;
		       eina_strbuf_append(txt, s);
		    }
		  else if (n == n2)
		    {
		       eina_strbuf_append_n(txt, s, index);
		    }
		  else
		    {
		       eina_strbuf_append(txt, s);
		    }
	       }
	  }
	else if (n->text)
	  {
	     if (format == EVAS_TEXTBLOCK_TEXT_PLAIN)
	       {
		  s = (char *)eina_strbuf_string_get(n->text);
		  while (*s)
		    {
		       if (*s == '\n')
			 eina_strbuf_append_char(txt, '\n');
		       else if (*s == '\t')
			 eina_strbuf_append_char(txt, '\t');
		       s++;
		    }
	       }
	     else if (format == EVAS_TEXTBLOCK_TEXT_MARKUP)
	       {
		  size_t tag_len, replace_len = eina_strbuf_length_get(n->text);
		  const char *tag = _style_match_replace(o->style, eina_strbuf_string_get(n->text), replace_len, &tag_len);
		  eina_strbuf_append_char(txt, '<');
		  if (tag)
		    {
		       // FIXME: need to escape
		       eina_strbuf_append_length(txt, tag, tag_len);
		    }
		  else
		    {
		       int push = 0;
		       int pop = 0;

		       // FIXME: need to escape
		       s = (char *)eina_strbuf_string_get(n->text);
		       if (*s == '+') push = 1;
		       if (*s == '-') pop = 1;
		       while ((*s == ' ') || (*s == '+') || (*s == '-')) s++;
		       if (pop) eina_strbuf_append_char(txt, '/');
		       if (push) eina_strbuf_append(txt, "+ ");
		       eina_strbuf_append(txt, s);
		    }
		  eina_strbuf_append_char(txt, '>');
	       }
	  }
	if (n == n2) break;
     }
   ret = eina_strbuf_string_steal(txt);
   eina_strbuf_free(txt);
   return ret;
}

/**
 * to be documented.
 * @param cur to be documented.
 * @param cx to be documented.
 * @param cy to be documented.
 * @param cw to be documented.
 * @param ch to be documented.
 * @return to be documented.
 */
EAPI int
evas_textblock_cursor_char_geometry_get(const Evas_Textblock_Cursor *cur, Evas_Coord *cx, Evas_Coord *cy, Evas_Coord *cw, Evas_Coord *ch)
{
   Evas_Object_Textblock *o;
   Evas_Object_Textblock_Line *ln = NULL;
   Evas_Object_Textblock_Item *it = NULL;
   Evas_Object_Textblock_Format_Item *fi = NULL;
   int x = 0, y = 0, w = 0, h = 0;
   int pos, ret;

   if (!cur) return -1;
   o = (Evas_Object_Textblock *)(cur->obj->object_data);
   if (!cur->node)
     {
        if (!o->nodes)
          {
             ln = o->lines;
             if (!ln) return -1;
             if (cx) *cx = ln->x;
             if (cy) *cy = ln->y;
             if (cw) *cw = ln->w;
             if (ch) *ch = ln->h;
             return ln->line_no;
          }
        else
          return -1;
     }
   if (!o->formatted.valid) _relayout(cur->obj);
   if (cur->node->type == NODE_FORMAT)
     {
	_find_layout_format_item_line_match(cur->obj, cur->node, &ln, &fi);
     }
   else
     {
	_find_layout_item_line_match(cur->obj, cur->node, cur->pos, cur->eol, &ln, &it);
     }
   if (!ln)
     {
	return -1;
     }
   if (it)
     {
	pos = cur->pos - it->source_pos;
	ret = -1;
        if (cur->eol)
          {
             int pos2;

             pos2 = pos;
             evas_common_font_utf8_get_next((unsigned char *)(it->text), &pos2);
             if (pos2 > pos) pos = pos2;
          }
        if (pos < 0) pos = 0;
	if (it->format->font.font)
	  ret = cur->ENFN->font_char_coords_get(cur->ENDT, it->format->font.font,
						it->text,
						pos,
						&x, &y, &w, &h);
	if (ret <= 0)
	  {
	     if (it->format->font.font)
	        cur->ENFN->font_string_size_get(cur->ENDT, it->format->font.font,
						it->text, &w, &h);
	     x = w;
	     y = 0;
	     w = 0;
	  }
	x = ln->x + it->x - it->inset + x;
	if (x < ln->x)
	  {
	     x = ln->x;
	     w -= (ln->x - x);
	  }
	y = ln->y;
	h = ln->h;
     }
   else if (fi)
     {
	x = ln->x + fi->x;
	y = ln->y;
	w = fi->w;
	h = ln->h;
     }
   else
     {
	return -1;
     }
   if (cx) *cx = x;
   if (cy) *cy = y;
   if (cw) *cw = w;
   if (ch) *ch = h;
   return ln->line_no;
}

/**
 * to be documented.
 * @param cur to be documented.
 * @param cx to be documented.
 * @param cy to be documented.
 * @param cw to be documented.
 * @param ch to be documented.
 * @return to be documented.
 */
EAPI int
evas_textblock_cursor_line_geometry_get(const Evas_Textblock_Cursor *cur, Evas_Coord *cx, Evas_Coord *cy, Evas_Coord *cw, Evas_Coord *ch)
{
   Evas_Object_Textblock *o;
   Evas_Object_Textblock_Line *ln = NULL;
   Evas_Object_Textblock_Item *it = NULL;
   Evas_Object_Textblock_Format_Item *fi = NULL;
   int x, y, w, h;

   if (!cur) return -1;
   o = (Evas_Object_Textblock *)(cur->obj->object_data);
   if (!o->formatted.valid) _relayout(cur->obj);
   if (!cur->node)
     {
        ln = o->lines;
     }
   else
     {
        if (cur->node->type == NODE_FORMAT)
          _find_layout_format_item_line_match(cur->obj, cur->node, &ln, &fi);
        else
          _find_layout_item_line_match(cur->obj, cur->node, cur->pos, cur->eol, &ln, &it);
     }
   if (!ln) return -1;
   x = ln->x;
   y = ln->y;
   w = ln->w;
   h = ln->h;
   if (cx) *cx = x;
   if (cy) *cy = y;
   if (cw) *cw = w;
   if (ch) *ch = h;
   return ln->line_no;
}

/**
 * to be documented.
 * @param cur to be documented.
 * @param x to be documented.
 * @param y to be documented.
 * @return to be documented.
 */
EAPI Eina_Bool
evas_textblock_cursor_char_coord_set(Evas_Textblock_Cursor *cur, Evas_Coord x, Evas_Coord y)
{
   Evas_Object_Textblock *o;
   Evas_Object_Textblock_Line *ln;
   Evas_Object_Textblock_Item *it = NULL, *it_break = NULL;
   Evas_Object_Textblock_Format_Item *fi = NULL;

   if (!cur) return EINA_FALSE;
   o = (Evas_Object_Textblock *)(cur->obj->object_data);
   if (!o->formatted.valid) _relayout(cur->obj);
   x += o->style_pad.l;
   y += o->style_pad.t;
   EINA_INLIST_FOREACH(o->lines, ln)
     {
	if (ln->y > y) break;
	if ((ln->y <= y) && ((ln->y + ln->h) > y))
	  {
	     EINA_INLIST_FOREACH(ln->items, it)
	       {
		  if ((it->x + ln->x) > x)
		    {
		       it_break = it;
		       break;
		    }
		  if (((it->x + ln->x) <= x) && (((it->x + ln->x) + it->w) > x))
		    {
		       int pos;
		       int cx, cy, cw, ch;

		       pos = -1;
		       if (it->format->font.font)
			 pos = cur->ENFN->font_char_at_coords_get(cur->ENDT,
								  it->format->font.font,
								  it->text,
								  x - it->x - ln->x, 0,
								  &cx, &cy, &cw, &ch);
		       if (pos < 0)
			 return EINA_FALSE;
		       cur->pos = pos + it->source_pos;
		       cur->node = it->source_node;
		       return 1;
		    }
	       }
	     EINA_INLIST_FOREACH(ln->format_items, fi)
	       {
		  if ((fi->x + ln->x) > x) break;
		  if (((fi->x + ln->x) <= x) && (((fi->x + ln->x) + fi->w) > x))
		    {
		       cur->pos = 0;
                       cur->eol = 0;
		       cur->node = fi->source_node;
		       return EINA_TRUE;
		    }
	       }
	     if (it_break)
	       {
		  it = it_break;
		  cur->pos = it->source_pos;
                  cur->eol = 0;
		  cur->node = it->source_node;
		  return EINA_TRUE;
	       }
	  }
     }
   return EINA_FALSE;
}

/**
 * to be documented.
 * @param cur to be documented.
 * @param y to be documented.
 * @return to be documented.
 */
EAPI int
evas_textblock_cursor_line_coord_set(Evas_Textblock_Cursor *cur, Evas_Coord y)
{
   Evas_Object_Textblock *o;
   Evas_Object_Textblock_Line *ln;

   if (!cur) return -1;
   o = (Evas_Object_Textblock *)(cur->obj->object_data);
   if (!o->formatted.valid) _relayout(cur->obj);
   y += o->style_pad.t;
   EINA_INLIST_FOREACH(o->lines, ln)
     {
	if (ln->y > y) break;
	if ((ln->y <= y) && ((ln->y + ln->h) > y))
	  {
	     evas_textblock_cursor_line_set(cur, ln->line_no);
	     return ln->line_no;
	  }
     }
   return -1;
}

/**
 * to be documented.
 * @param cur1 to be documented.
 * @param cur2 to be documented.
 * @return to be documented.
 */
EAPI Eina_List *
evas_textblock_cursor_range_geometry_get(const Evas_Textblock_Cursor *cur1, const Evas_Textblock_Cursor *cur2)
{
   Eina_List *rects = NULL;
   Evas_Coord cx, cy, cw, ch, lx, ly, lw, lh;
   Evas_Textblock_Rectangle *tr;
   int i, line, line2;

   if (!cur1) return NULL;
   if (!cur2) return NULL;
   if (cur1->obj != cur2->obj) return NULL;
   if (evas_textblock_cursor_compare(cur1, cur2) > 0)
     {
	const Evas_Textblock_Cursor *tc;

	tc = cur1;
	cur1 = cur2;
	cur2 = tc;
     }
   line = evas_textblock_cursor_char_geometry_get(cur1, &cx, &cy, &cw, &ch);
   if (line < 0) return NULL;
   line = evas_textblock_cursor_line_geometry_get(cur1, &lx, &ly, &lw, &lh);
   if (line < 0) return NULL;
   line2 = evas_textblock_cursor_line_geometry_get(cur2, NULL, NULL, NULL, NULL);
   if (line2 < 0) return NULL;
   if (line == line2)
     {
	tr = calloc(1, sizeof(Evas_Textblock_Rectangle));
	rects = eina_list_append(rects, tr);
	tr->x = cx;
	tr->y = ly;
	tr->h = lh;
	line = evas_textblock_cursor_char_geometry_get(cur2, &cx, &cy, &cw, &ch);
	if (line < 0)
	  {
	     while (rects)
	       {
		  free(rects->data);
		  rects = eina_list_remove_list(rects, rects);
	       }
	     return NULL;
	  }
	tr->w = cx + cw - tr->x;
     }
   else
     {
	tr = calloc(1, sizeof(Evas_Textblock_Rectangle));
	rects = eina_list_append(rects, tr);
	tr->x = cx;
	tr->y = ly;
	tr->h = lh;
	tr->w = lx + lw - cx;
	for (i = line +1; i < line2; i++)
	  {
	     evas_object_textblock_line_number_geometry_get(cur1->obj, i, &lx, &ly, &lw, &lh);
	     tr = calloc(1, sizeof(Evas_Textblock_Rectangle));
	     rects = eina_list_append(rects, tr);
	     tr->x = lx;
	     tr->y = ly;
	     tr->h = lh;
	     tr->w = lw;
	  }
	line = evas_textblock_cursor_char_geometry_get(cur2, &cx, &cy, &cw, &ch);
	if (line < 0)
	  {
	     while (rects)
	       {
		  free(rects->data);
		  rects = eina_list_remove_list(rects, rects);
	       }
	     return NULL;
	  }
	line = evas_textblock_cursor_line_geometry_get(cur2, &lx, &ly, &lw, &lh);
	if (line < 0)
	  {
	     while (rects)
	       {
		  free(rects->data);
		  rects = eina_list_remove_list(rects, rects);
	       }
	     return NULL;
	  }
	tr = calloc(1, sizeof(Evas_Textblock_Rectangle));
	rects = eina_list_append(rects, tr);
	tr->x = lx;
	tr->y = ly;
	tr->h = lh;
	tr->w = cx + cw - lx;
     }
   return rects;
}

/**
 * to be documented.
 * @param cur to be documented.
 * @param cx to be documented.
 * @param cy to be documented.
 * @param cw to be documented.
 * @param ch to be documented.
 * @return to be documented.
 */
EAPI Eina_Bool
evas_textblock_cursor_format_item_geometry_get(const Evas_Textblock_Cursor *cur, Evas_Coord *cx, Evas_Coord *cy, Evas_Coord *cw, Evas_Coord *ch)
{
   Evas_Object_Textblock *o;
   Evas_Object_Textblock_Line *ln = NULL;
   Evas_Object_Textblock_Format_Item *fi = NULL;
   Evas_Coord x, y, w, h;

   if (!cur) return 0;
   o = (Evas_Object_Textblock *)(cur->obj->object_data);
   if (!o->formatted.valid) _relayout(cur->obj);
   _find_layout_format_item_line_match(cur->obj, cur->node, &ln, &fi);
   if ((!ln) || (!fi)) return 0;
   x = ln->x + fi->x;
   y = ln->y + ln->baseline + fi->y;
   w = fi->w;
   h = fi->h;
   if (cx) *cx = x;
   if (cy) *cy = y;
   if (cw) *cw = w;
   if (ch) *ch = h;
   return 1;
}

/**
 * To be documented.
 *
 * FIXME: To be fixed.
 *
 */
EAPI Eina_Bool
evas_textblock_cursor_eol_get(const Evas_Textblock_Cursor *cur)
{
   if (!cur) return EINA_FALSE;
   return cur->eol;
}

/**
 * To be documented.
 *
 * FIXME: To be fixed.
 *
 */
EAPI void
evas_textblock_cursor_eol_set(Evas_Textblock_Cursor *cur, Eina_Bool eol)
{
   if (!cur) return;
   cur->eol = eol;
}

/* }}} */

/* general controls */
/**
 * to be documented.
 * @param obj to be documented.
 * @param line to be documented.
 * @param cx to be documented.
 * @param cy to be documented.
 * @param cw to be documented.
 * @param ch to be documented.
 * @return to be documented.
 */
EAPI Eina_Bool
evas_object_textblock_line_number_geometry_get(const Evas_Object *obj, int line, Evas_Coord *cx, Evas_Coord *cy, Evas_Coord *cw, Evas_Coord *ch)
{
   Evas_Object_Textblock_Line *ln;

   TB_HEAD_RETURN(0);
   ln = _find_layout_line_num(obj, line);
   if (!ln) return EINA_FALSE;
   if (cx) *cx = ln->x;
   if (cy) *cy = ln->y;
   if (cw) *cw = ln->w;
   if (ch) *ch = ln->h;
   return EINA_TRUE;
}

/**
 * to be documented.
 * @param obj to be documented.
 * @return Returns no value.
 */
EAPI void
evas_object_textblock_clear(Evas_Object *obj)
{
   Eina_List *l;
   Evas_Textblock_Cursor *cur;

   TB_HEAD();
   _nodes_clear(obj);
   EINA_LIST_FOREACH(o->cursors, l, cur)
     {
	cur->node = NULL;
	cur->pos = 0;
        cur->eol = 0;
     }
   if (o->lines)
     {
	_lines_clear(obj, o->lines);
	o->lines = NULL;
     }
   o->formatted.valid = 0;
   o->native.valid = 0;
   o->changed = 1;
   if (o->markup_text)
     {
	free(o->markup_text);
	o->markup_text = NULL;
     }
   evas_object_change(obj);
   /* FIXME: adjust cursors that are affected by the change */
}

/**
 * to be documented.
 * @param obj to be documented.
 * @param w to be documented.
 * @param h to be documented.
 * @return Returns no value.
 */
EAPI void
evas_object_textblock_size_formatted_get(const Evas_Object *obj, Evas_Coord *w, Evas_Coord *h)
{
   TB_HEAD();
   if (!o->formatted.valid) _relayout(obj);
   if (w) *w = o->formatted.w;
   if (h) *h = o->formatted.h;
}

/**
 * to be documented.
 * @param obj to be documented.
 * @param w to be documented.
 * @param h to be documented.
 * @return Returns no value.
 */
EAPI void
evas_object_textblock_size_native_get(const Evas_Object *obj, Evas_Coord *w, Evas_Coord *h)
{
   TB_HEAD();
   if (!o->native.valid)
     {
	_layout(obj,
		1,
		-1, -1,
		&o->native.w, &o->native.h);
	o->native.valid = 1;
     }
   if (w) *w = o->native.w;
   if (h) *h = o->native.h;
}

/**
 * to be documented.
 * @param obj to be documented.
 * @param l to be documented.
 * @param r to be documented.
 * @param t to be documented.
 * @param b to be documented.
 * @return Returns no value.
 */
EAPI void
evas_object_textblock_style_insets_get(const Evas_Object *obj, Evas_Coord *l, Evas_Coord *r, Evas_Coord *t, Evas_Coord *b)
{
   TB_HEAD();
   if (!o->formatted.valid) _relayout(obj);
   if (l) *l = o->style_pad.l;
   if (r) *r = o->style_pad.r;
   if (t) *t = o->style_pad.t;
   if (b) *b = o->style_pad.b;
}

/**
 * @}
 */

/* all nice and private */
static void
evas_object_textblock_init(Evas_Object *obj)
{
   Evas_Object_Textblock *o;

   /* alloc image ob, setup methods and default values */
   obj->object_data = evas_object_textblock_new();
   /* set up default settings for this kind of object */
   obj->cur.color.r = 255;
   obj->cur.color.g = 255;
   obj->cur.color.b = 255;
   obj->cur.color.a = 255;
   obj->cur.geometry.x = 0.0;
   obj->cur.geometry.y = 0.0;
   obj->cur.geometry.w = 0.0;
   obj->cur.geometry.h = 0.0;
   obj->cur.layer = 0;
   /* set up object-specific settings */
   obj->prev = obj->cur;
   /* set up methods (compulsory) */
   obj->func = &object_func;
   obj->type = o_type;

   o = (Evas_Object_Textblock *)(obj->object_data);
   o->cursor->obj = obj;
}

static void *
evas_object_textblock_new(void)
{
   Evas_Object_Textblock *o;

   /* alloc obj private data */
   o = calloc(1, sizeof(Evas_Object_Textblock));
   o->magic = MAGIC_OBJ_TEXTBLOCK;
   o->cursor = calloc(1, sizeof(Evas_Textblock_Cursor));
   o->cursors = eina_list_append(NULL, o->cursor);
   return o;
}

static void
evas_object_textblock_free(Evas_Object *obj)
{
   Evas_Object_Textblock *o;

   evas_object_textblock_clear(obj);
   evas_object_textblock_style_set(obj, NULL);
   o = (Evas_Object_Textblock *)(obj->object_data);
   while (o->cursors)
     {
	Evas_Textblock_Cursor *cur;

	cur = (Evas_Textblock_Cursor *)o->cursors->data;
	o->cursors = eina_list_remove_list(o->cursors, o->cursors);
	free(cur);
     }
   if (o->repch) eina_stringshare_del(o->repch);
   o->magic = 0;
   free(o);
}

static void
evas_object_textblock_render(Evas_Object *obj, void *output, void *context, void *surface, int x, int y)
{
   Evas_Object_Textblock_Line *ln;
   Evas_Object_Textblock *o;
   int i, j;
   int pback = 0, backx = 0;
   int pline = 0, linex = 0;
   int pline2 = 0, line2x = 0;
   int pstrike = 0, strikex = 0;
   int x2;
   unsigned char r = 0, g = 0, b = 0, a = 0;
   unsigned char r2 = 0, g2 = 0, b2 = 0, a2 = 0;
   unsigned char r3 = 0, g3 = 0, b3 = 0, a3 = 0;
   int cx, cy, cw, ch, clip;
   const char vals[5][5] =
     {
	  {0, 1, 2, 1, 0},
	  {1, 3, 4, 3, 1},
	  {2, 4, 5, 4, 2},
	  {1, 3, 4, 3, 1},
	  {0, 1, 2, 1, 0}
     };

   /* render object to surface with context, and offxet by x,y */
   o = (Evas_Object_Textblock *)(obj->object_data);
   obj->layer->evas->engine.func->context_multiplier_unset(output,
							   context);
   clip = ENFN->context_clip_get(output, context, &cx, &cy, &cw, &ch);
#define ITEM_WALK() \
   EINA_INLIST_FOREACH(o->lines, ln) \
     { \
	Evas_Object_Textblock_Item *it; \
	\
        pback = 0; \
        pline = 0; \
        pline2 = 0; \
	pstrike = 0; \
        if (clip) \
          { \
             if ((obj->cur.geometry.y + y + ln->y + ln->h) < (cy - 20)) \
               continue; \
             if ((obj->cur.geometry.y + y + ln->y) > (cy + ch + 20)) \
               break; \
          } \
	EINA_INLIST_FOREACH(ln->items, it) \
	  { \
	     int yoff; \
	     \
	     yoff = ln->baseline; \
	     if (it->format->valign != -1.0) \
	       yoff = (it->format->valign * (double)(ln->h - it->h)) + it->baseline; \
             if (clip) \
               { \
                  if ((obj->cur.geometry.x + x + ln->x + it->x - it->inset + it->w) < (cx - 20)) \
                    continue; \
                  if ((obj->cur.geometry.x + x + ln->x + it->x - it->inset) > (cx + cw + 20)) \
                    break; \
               }
             
#define ITEM_WALK_END() \
	  } \
     }
#define COLOR_SET(col) \
	ENFN->context_color_set(output, context, \
				(obj->cur.cache.clip.r * it->format->color.col.r) / 255, \
				(obj->cur.cache.clip.g * it->format->color.col.g) / 255, \
				(obj->cur.cache.clip.b * it->format->color.col.b) / 255, \
				(obj->cur.cache.clip.a * it->format->color.col.a) / 255);
#define COLOR_SET_AMUL(col, amul) \
	ENFN->context_color_set(output, context, \
				(obj->cur.cache.clip.r * it->format->color.col.r * (amul)) / 65025, \
				(obj->cur.cache.clip.g * it->format->color.col.g * (amul)) / 65025, \
				(obj->cur.cache.clip.b * it->format->color.col.b * (amul)) / 65025, \
				(obj->cur.cache.clip.a * it->format->color.col.a * (amul)) / 65025);
#define DRAW_TEXT(ox, oy) \
   if (it->format->font.font) ENFN->font_draw(output, context, surface, it->format->font.font, \
						 obj->cur.geometry.x + ln->x + it->x - it->inset + x + (ox), \
						 obj->cur.geometry.y + ln->y + yoff + y + (oy), \
						 it->w, it->h, it->w, it->h, it->text);
#if 0
//#define DRAW_TEXT(ox, oy) \
//   if (it->format->font.font) ENFN->font_draw(output, context, surface, it->format->font.font, \
//						 obj->cur.geometry.x + ln->x + it->x - it->inset + x + (ox), \
//						 obj->cur.geometry.y + ln->y + yoff + y + (oy), \
//						 obj->cur.cache.geometry.x + ln->x + it->x - it->inset + x + (ox), \
//						 obj->cur.cache.geometry.y + ln->y + yoff + y + (oy), \
//						 it->w, it->h, it->w, it->h, it->text);
#endif
#define ITEM_WALK_LINE_SKIP_DROP() \
   if ((ln->y + ln->h) <= 0) continue; \
   if (ln->y > obj->cur.geometry.h) break

#ifdef DEBUG_TEXTBLOCK
   ITEM_WALK();

   ENFN->context_color_set(output, context,
                           0, 0, 255, 255);
   ENFN->rectangle_draw(output, context, surface,
                        obj->cur.geometry.x + ln->x + x,
                        obj->cur.geometry.y + ln->y + y,
                        ln->w, 1);
   ENFN->rectangle_draw(output, context, surface,
                        obj->cur.geometry.x + ln->x + x,
                        obj->cur.geometry.y + ln->y + y,
                        1, ln->h);
   ENFN->rectangle_draw(output, context, surface,
                        obj->cur.geometry.x + ln->x + x,
                        obj->cur.geometry.y + ln->y + y + ln->h - 1,
                        ln->w, 1);
   ENFN->rectangle_draw(output, context, surface,
                        obj->cur.geometry.x + ln->x + x + ln->w - 1,
                        obj->cur.geometry.y + ln->y + y,
                        1, ln->h);
   ENFN->context_color_set(output, context, 255, 0, 0, 255);
   ENFN->rectangle_draw(output, context, surface,
                        obj->cur.geometry.x + ln->x + x,
                        obj->cur.geometry.y + ln->y + y + ln->baseline,
                        ln->w, 1);

   ITEM_WALK_END();
#endif

   pback = 0;
   /* backing */
   ITEM_WALK();
   ITEM_WALK_LINE_SKIP_DROP();
   if ((it->format->backing) && (!pback) && ((EINA_INLIST_GET(it))->next))
     {
	pback = 1;
	backx = it->x;
	r = it->format->color.backing.r;
	g = it->format->color.backing.g;
	b = it->format->color.backing.b;
	a = it->format->color.backing.a;
     }
   else if (((pback) && (!it->format->backing)) ||
	    (!(EINA_INLIST_GET(it))->next) ||
	    (it->format->color.backing.r != r) ||
	    (it->format->color.backing.g != g) ||
	    (it->format->color.backing.b != b) ||
	    (it->format->color.backing.a != a))
     {
        if ((it->format->backing) && (!pback) && (!(EINA_INLIST_GET(it))->next))
          {
             r = it->format->color.backing.r;
             g = it->format->color.backing.g;
             b = it->format->color.backing.b;
             a = it->format->color.backing.a;
             pback = 1;
             backx = it->x;
          }
        if (!it->format->backing) x2 = it->x;
        else x2 = it->x + it->w;
	if ((pback) && (x2 > backx))
	  {
	     ENFN->context_color_set(output,
				     context,
				     (obj->cur.cache.clip.r * r) / 255,
				     (obj->cur.cache.clip.g * g) / 255,
				     (obj->cur.cache.clip.b * b) / 255,
				     (obj->cur.cache.clip.a * a) / 255);
	     ENFN->rectangle_draw(output,
				  context,
				  surface,
				  obj->cur.geometry.x + ln->x + backx + x,
				  obj->cur.geometry.y + ln->y + y,
////				  obj->cur.cache.geometry.x + ln->x + backx + x,
////				  obj->cur.cache.geometry.y + ln->y + y,
				  x2 - backx,
				  ln->h);
	  }
        pback = it->format->backing;
	backx = it->x;
	r = it->format->color.backing.r;
	g = it->format->color.backing.g;
	b = it->format->color.backing.b;
	a = it->format->color.backing.a;
     }
   ITEM_WALK_END();

   /* shadows */
   ITEM_WALK();
   ITEM_WALK_LINE_SKIP_DROP();
   if (it->format->style == EVAS_TEXT_STYLE_SHADOW)
     {
	COLOR_SET(shadow);
	DRAW_TEXT(1, 1);
     }
   else if ((it->format->style == EVAS_TEXT_STYLE_OUTLINE_SHADOW) ||
	    (it->format->style == EVAS_TEXT_STYLE_FAR_SHADOW))
     {
	COLOR_SET(shadow);
	DRAW_TEXT(2, 2);
     }
   else if ((it->format->style == EVAS_TEXT_STYLE_OUTLINE_SOFT_SHADOW) ||
	    (it->format->style == EVAS_TEXT_STYLE_FAR_SOFT_SHADOW))
     {
	for (j = 0; j < 5; j++)
	  {
	     for (i = 0; i < 5; i++)
	       {
		  if (vals[i][j] != 0)
		    {
		       COLOR_SET_AMUL(shadow, vals[i][j] * 50);
		       DRAW_TEXT(i, j);
		    }
	       }
	  }
     }
   else if (it->format->style == EVAS_TEXT_STYLE_SOFT_SHADOW)
     {
	for (j = 0; j < 5; j++)
	  {
	     for (i = 0; i < 5; i++)
	       {
		  if (vals[i][j] != 0)
		    {
		       COLOR_SET_AMUL(shadow, vals[i][j] * 50);
		       DRAW_TEXT(i - 1, j - 1);
		    }
	       }
	  }
     }
   ITEM_WALK_END();

   /* glows */
   ITEM_WALK();
   ITEM_WALK_LINE_SKIP_DROP();
   if (it->format->style == EVAS_TEXT_STYLE_GLOW)
     {
	for (j = 0; j < 5; j++)
	  {
	     for (i = 0; i < 5; i++)
	       {
		  if (vals[i][j] != 0)
		    {
		       COLOR_SET_AMUL(glow, vals[i][j] * 50);
		       DRAW_TEXT(i - 2, j - 2);
		    }
	       }
	  }
	COLOR_SET(glow2);
	DRAW_TEXT(-1, 0);
	DRAW_TEXT(1, 0);
	DRAW_TEXT(0, -1);
	DRAW_TEXT(0, 1);
     }
   ITEM_WALK_END();

   /* outlines */
   ITEM_WALK();
   ITEM_WALK_LINE_SKIP_DROP();
   if ((it->format->style == EVAS_TEXT_STYLE_OUTLINE) ||
       (it->format->style == EVAS_TEXT_STYLE_OUTLINE_SHADOW) ||
       (it->format->style == EVAS_TEXT_STYLE_OUTLINE_SOFT_SHADOW))
     {
	COLOR_SET(outline);
	DRAW_TEXT(-1, 0);
	DRAW_TEXT(1, 0);
	DRAW_TEXT(0, -1);
	DRAW_TEXT(0, 1);
     }
   else if (it->format->style == EVAS_TEXT_STYLE_SOFT_OUTLINE)
     {
	for (j = 0; j < 5; j++)
	  {
	     for (i = 0; i < 5; i++)
	       {
		  if (((i != 2) || (j != 2)) && (vals[i][j] != 0))
		    {
		       COLOR_SET_AMUL(outline, vals[i][j] * 50);
		       DRAW_TEXT(i - 2, j - 2);
		    }
	       }
	  }
     }
   ITEM_WALK_END();

   /* normal text */
   ITEM_WALK();
   ITEM_WALK_LINE_SKIP_DROP();
   COLOR_SET(normal);
   DRAW_TEXT(0, 0);
   if ((it->format->strikethrough) && (!pstrike) && ((EINA_INLIST_GET(it))->next))
     {
	pstrike = 1;
	strikex = it->x;
	r3 = it->format->color.strikethrough.r;
	g3 = it->format->color.strikethrough.g;
	b3 = it->format->color.strikethrough.b;
	a3 = it->format->color.strikethrough.a;
     }
   else if (((pstrike) && (!it->format->strikethrough)) ||
	    (!(EINA_INLIST_GET(it))->next) ||
	    (it->format->color.strikethrough.r != r3) ||
	    (it->format->color.strikethrough.g != g3) ||
	    (it->format->color.strikethrough.b != b3) ||
	    (it->format->color.strikethrough.a != a3))
     {
	if ((it->format->strikethrough) && (!pstrike))
	  {
	     strikex = it->x;
	     r3 = it->format->color.strikethrough.r;
	     g3 = it->format->color.strikethrough.g;
	     b3 = it->format->color.strikethrough.b;
	     a3 = it->format->color.strikethrough.a;
	  }
	x2 = it->x + it->w;
	if (!it->format->strikethrough)
	  {
	     x2 = it->x;
	     pstrike = 0;
	  }
	if (x2 > strikex)
	  {
	     ENFN->context_color_set(output,
				     context,
				     (obj->cur.cache.clip.r * r3) / 255,
				     (obj->cur.cache.clip.g * g3) / 255,
				     (obj->cur.cache.clip.b * b3) / 255,
				     (obj->cur.cache.clip.a * a3) / 255);
	     ENFN->rectangle_draw(output,
				  context,
				  surface,
				  obj->cur.geometry.x + ln->x + strikex + x,
				  obj->cur.geometry.y + ln->y + y + (ln->h / 2),
////				  obj->cur.cache.geometry.x + ln->x + strikex + x,
////				  obj->cur.cache.geometry.y + ln->y + y + (ln->h / 2),
				  x2 - strikex,
				  1);
	  }
	if (it->format->strikethrough) pstrike = 1;
	strikex = it->x;
	r3 = it->format->color.strikethrough.r;
	g3 = it->format->color.strikethrough.g;
	b3 = it->format->color.strikethrough.b;
	a3 = it->format->color.strikethrough.a;
     }
   if ((it->format->underline) && (!pline) && ((EINA_INLIST_GET(it))->next))
     {
	pline = 1;
	linex = it->x;
	r = it->format->color.underline.r;
	g = it->format->color.underline.g;
	b = it->format->color.underline.b;
	a = it->format->color.underline.a;
     }
   else if (((pline) && (!it->format->underline)) ||
	    (!(EINA_INLIST_GET(it))->next) ||
	    (it->format->color.underline.r != r) ||
	    (it->format->color.underline.g != g) ||
	    (it->format->color.underline.b != b) ||
	    (it->format->color.underline.a != a))
     {
	if ((it->format->underline) && (!pline))
	  {
	     linex = it->x;
	     r = it->format->color.underline.r;
	     g = it->format->color.underline.g;
	     b = it->format->color.underline.b;
	     a = it->format->color.underline.a;
	  }
	x2 = it->x + it->w;
	if (!it->format->underline)
	  {
	     x2 = it->x;
	     pline = 0;
	  }
	if (x2 > linex)
	  {
	     ENFN->context_color_set(output,
				     context,
				     (obj->cur.cache.clip.r * r) / 255,
				     (obj->cur.cache.clip.g * g) / 255,
				     (obj->cur.cache.clip.b * b) / 255,
				     (obj->cur.cache.clip.a * a) / 255);
	     ENFN->rectangle_draw(output,
				  context,
				  surface,
				  obj->cur.geometry.x + ln->x + linex + x,
				  obj->cur.geometry.y + ln->y + y + ln->baseline + 1,
////				  obj->cur.cache.geometry.x + ln->x + linex + x,
////				  obj->cur.cache.geometry.y + ln->y + y + ln->baseline + 1,
				  x2 - linex,
				  1);
	  }
	if (it->format->underline) pline = 1;
	linex = it->x;
	r = it->format->color.underline.r;
	g = it->format->color.underline.g;
	b = it->format->color.underline.b;
	a = it->format->color.underline.a;
     }
   if ((it->format->underline2) && (!pline2) && ((EINA_INLIST_GET(it))->next))
     {
	pline2 = 1;
	line2x = it->x;
	r2 = it->format->color.underline2.r;
	g2 = it->format->color.underline2.g;
	b2 = it->format->color.underline2.b;
	a2 = it->format->color.underline2.a;
     }
   else if (((pline2) && (!it->format->underline2)) ||
	    (!(EINA_INLIST_GET(it))->next) ||
	    (it->format->color.underline2.r != r2) ||
	    (it->format->color.underline2.g != g2) ||
	    (it->format->color.underline2.b != b2) ||
	    (it->format->color.underline2.a != a2))
     {
	if ((it->format->underline2) && (!pline2))
	  {
	     line2x = it->x;
	     r2 = it->format->color.underline2.r;
	     g2 = it->format->color.underline2.g;
	     b2 = it->format->color.underline2.b;
	     a2 = it->format->color.underline2.a;
	  }
	x2 = it->x + it->w;
	if (!it->format->underline2)
	  {
	     x2 = it->x;
	     pline2 = 0;
	  }
	if (x2 > line2x)
	  {
	     ENFN->context_color_set(output,
				     context,
				     (obj->cur.cache.clip.r * r2) / 255,
				     (obj->cur.cache.clip.g * g2) / 255,
				     (obj->cur.cache.clip.b * b2) / 255,
				     (obj->cur.cache.clip.a * a2) / 255);
	     ENFN->rectangle_draw(output,
				  context,
				  surface,
				  obj->cur.geometry.x + ln->x + line2x + x,
				  obj->cur.geometry.y + ln->y + y + ln->baseline + 3,
////				  obj->cur.cache.geometry.x + ln->x + line2x + x,
////				  obj->cur.cache.geometry.y + ln->y + y + ln->baseline + 3,
				  x2 - line2x,
				  1);
	  }
	if (it->format->underline2) pline2 = 1;
	line2x = it->x;
	r2 = it->format->color.underline2.r;
	g2 = it->format->color.underline2.g;
	b2 = it->format->color.underline2.b;
	a2 = it->format->color.underline2.a;
     }
   ITEM_WALK_END();

}

static void
evas_object_textblock_render_pre(Evas_Object *obj)
{
   Evas_Object_Textblock *o;
   int is_v, was_v;

   /* dont pre-render the obj twice! */
   if (obj->pre_render_done) return;
   obj->pre_render_done = 1;
   /* pre-render phase. this does anything an object needs to do just before */
   /* rendering. this could mean loading the image data, retrieving it from */
   /* elsewhere, decoding video etc. */
   /* then when this is done the object needs to figure if it changed and */
   /* if so what and where and add the appropriate redraw textblocks */
   o = (Evas_Object_Textblock *)(obj->object_data);
   if ((o->changed) ||
       (o->last_w != obj->cur.geometry.w))
     {
	Evas_Object_Textblock_Line *lines;

	lines = o->lines;
	o->lines = NULL;
	o->formatted.valid = 0;
	o->native.valid = 0;
	_layout(obj,
		0,
		obj->cur.geometry.w, obj->cur.geometry.h,
		&o->formatted.w, &o->formatted.h);
	o->formatted.valid = 1;
	if (lines) _lines_clear(obj, lines);
	o->last_w = obj->cur.geometry.w;
	o->redraw = 0;
	evas_object_render_pre_prev_cur_add(&obj->layer->evas->clip_changes, obj);
	o->changed = 0;
	is_v = evas_object_is_visible(obj);
	was_v = evas_object_was_visible(obj);
	goto done;
     }
   if (o->redraw)
     {
	o->redraw = 0;
	evas_object_render_pre_prev_cur_add(&obj->layer->evas->clip_changes, obj);
	o->changed = 0;
	is_v = evas_object_is_visible(obj);
	was_v = evas_object_was_visible(obj);
	goto done;
     }
   /* if someone is clipping this obj - go calculate the clipper */
   if (obj->cur.clipper)
     {
	if (obj->cur.cache.clip.dirty)
	  evas_object_clip_recalc(obj->cur.clipper);
	obj->cur.clipper->func->render_pre(obj->cur.clipper);
     }
   /* now figure what changed and add draw rects */
   /* if it just became visible or invisible */
   is_v = evas_object_is_visible(obj);
   was_v = evas_object_was_visible(obj);
   if (is_v != was_v)
     {
	evas_object_render_pre_visible_change(&obj->layer->evas->clip_changes, obj, is_v, was_v);
	goto done;
     }
   if ((obj->cur.map != obj->prev.map) ||
       (obj->cur.usemap != obj->prev.usemap))
     {
	evas_object_render_pre_prev_cur_add(&obj->layer->evas->clip_changes, obj);
        goto done;
     }
   /* it's not visible - we accounted for it appearing or not so just abort */
   if (!is_v) goto done;
   /* clipper changed this is in addition to anything else for obj */
   evas_object_render_pre_clipper_change(&obj->layer->evas->clip_changes, obj);
   /* if we restacked (layer or just within a layer) and don't clip anyone */
   if (obj->restack)
     {
	evas_object_render_pre_prev_cur_add(&obj->layer->evas->clip_changes, obj);
	goto done;
     }
   /* if it changed color */
   if ((obj->cur.color.r != obj->prev.color.r) ||
       (obj->cur.color.g != obj->prev.color.g) ||
       (obj->cur.color.b != obj->prev.color.b) ||
       (obj->cur.color.a != obj->prev.color.a))
     {
	evas_object_render_pre_prev_cur_add(&obj->layer->evas->clip_changes, obj);
	goto done;
     }
   /* if it changed geometry - and obviously not visibility or color */
   /* caluclate differences since we have a constant color fill */
   /* we really only need to update the differences */
   if ((obj->cur.geometry.x != obj->prev.geometry.x) ||
       (obj->cur.geometry.y != obj->prev.geometry.y) ||
       (obj->cur.geometry.w != obj->prev.geometry.w) ||
       (obj->cur.geometry.h != obj->prev.geometry.h))
     {
	evas_object_render_pre_prev_cur_add(&obj->layer->evas->clip_changes, obj);
	goto done;
     }
   if (o->changed)
     {
	evas_object_render_pre_prev_cur_add(&obj->layer->evas->clip_changes, obj);
	o->changed = 0;
     }
   done:
   evas_object_render_pre_effect_updates(&obj->layer->evas->clip_changes, obj, is_v, was_v);
}

static void
evas_object_textblock_render_post(Evas_Object *obj)
{
/*   Evas_Object_Textblock *o; */

   /* this moves the current data to the previous state parts of the object */
   /* in whatever way is safest for the object. also if we don't need object */
   /* data anymore we can free it if the object deems this is a good idea */
/*   o = (Evas_Object_Textblock *)(obj->object_data); */
   /* remove those pesky changes */
   evas_object_clip_changes_clean(obj);
   /* move cur to prev safely for object data */
   obj->prev = obj->cur;
/*   o->prev = o->cur; */
/*   o->changed = 0; */
}

static unsigned int evas_object_textblock_id_get(Evas_Object *obj)
{
   Evas_Object_Textblock *o;

   o = (Evas_Object_Textblock *)(obj->object_data);
   if (!o) return 0;
   return MAGIC_OBJ_TEXTBLOCK;
}

static unsigned int evas_object_textblock_visual_id_get(Evas_Object *obj)
{
   Evas_Object_Textblock *o;

   o = (Evas_Object_Textblock *)(obj->object_data);
   if (!o) return 0;
   return MAGIC_OBJ_CUSTOM;
}

static void *evas_object_textblock_engine_data_get(Evas_Object *obj)
{
   Evas_Object_Textblock *o;

   o = (Evas_Object_Textblock *)(obj->object_data);
   if (!o) return NULL;
   return o->engine_data;
}

static int
evas_object_textblock_is_opaque(Evas_Object *obj)
{
   /* this returns 1 if the internal object data implies that the object is */
   /* currently fulyl opque over the entire gradient it occupies */
   return 0;
}

static int
evas_object_textblock_was_opaque(Evas_Object *obj)
{
   /* this returns 1 if the internal object data implies that the object was */
   /* currently fulyl opque over the entire gradient it occupies */
   return 0;
}

static void
evas_object_textblock_coords_recalc(Evas_Object *obj)
{
   Evas_Object_Textblock *o;

   o = (Evas_Object_Textblock *)(obj->object_data);
   if (obj->cur.geometry.w != o->last_w)
     {
	o->formatted.valid = 0;
	o->native.valid = 0;
	o->changed = 1;
     }
}

static void
evas_object_textblock_scale_update(Evas_Object *obj)
{
   _relayout(obj);
}

void
_evas_object_textblock_rehint(Evas_Object *obj)
{
   Evas_Object_Textblock *o;
   Evas_Object_Textblock_Line *ln;

   o = (Evas_Object_Textblock *)(obj->object_data);
   EINA_INLIST_FOREACH(o->lines, ln)
     {
	Evas_Object_Textblock_Item *it;

	EINA_INLIST_FOREACH(ln->items, it)
	  {
	     if (it->format->font.font)
            {  
#ifdef EVAS_FRAME_QUEUING
               evas_common_pipe_op_text_flush(it->format->font.font);
#endif
	          evas_font_load_hinting_set(obj->layer->evas,
			   		  it->format->font.font,
					  obj->layer->evas->hinting);
	       }
	  }
     }
   o->formatted.valid = 0;
   o->native.valid = 0;
   o->changed = 1;
   evas_object_change(obj);
}
