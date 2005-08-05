/*
 * vim:ts=8:sw=3:sts=8:noexpandtab:cino=>5n-3f0^-2{2
 */

#include "evas_common.h"
#include "evas_private.h"

/* FIXME:
 *
 * this is a rewrite of textblock - for now api calsl are just textblock2
 * instead of textblock
 *
 */

/* save typing */
#define ENFN obj->layer->evas->engine.func
#define ENDT obj->layer->evas->engine.data.output

/* private magic number for textblock objects */
static const char o_type[] = "textblock";

/* private struct for textblock object internal data */
typedef struct _Evas_Object_Textblock       Evas_Object_Textblock;
typedef struct _Evas_Object_Style_Tag       Evas_Object_Style_Tag;
typedef struct _Evas_Object_Textblock_Node  Evas_Object_Textblock_Node;
typedef struct _Evas_Object_Textblock_Lnode Evas_Object_Textblock_Lnode;

/* the current state of the formatting */

struct _Evas_Object_Style_Tag
{
   Evas_Object_List _list_data;
   
   char *tag;
   char *replace;
};

#define  NODE_TEXT   0
#define  NODE_FORMAT 1

struct _Evas_Object_Textblock_Node
{
   Evas_Object_List _list_data;
   int   type;
   char *text;
   int   len, alloc;
};

struct _Evas_Object_Textblock_Lnode
{
   Evas_Object_List _list_data;
   int   type;
   char *text;
   int   len, alloc;
};

struct _Evas_Textblock_Style
{
   char                  *style_text;
   char                  *default_tag;
   Evas_Object_Style_Tag *tags;
   Evas_List             *objects;
   unsigned char          delete_me : 1;
};

struct _Evas_Textblock_Cursor
{
   Evas_Object                *obj;
   int                         pos;
   Evas_Object_Textblock_Node *node;
};

struct _Evas_Object_Textblock
{
   DATA32                       magic;
   struct {
      int                       dummy;
   } cur, prev;
   Evas_Textblock_Style        *style;
   Evas_Textblock_Cursor       *cursor;
   Evas_List                   *cursors;
   Evas_Object_Textblock_Node  *nodes;
   Evas_Object_Textblock_Lnode *lnodes;
   char                        *markup_text;
   char                         changed : 1;
   void                        *engine_data;
};

/* private methods for textblock objects */
static void evas_object_textblock_init(Evas_Object *obj);
static void *evas_object_textblock_new(void);
static void evas_object_textblock_render(Evas_Object *obj, void *output, void *context, void *surface, int x, int y);
static void evas_object_textblock_free(Evas_Object *obj);
static void evas_object_textblock_render_pre(Evas_Object *obj);
static void evas_object_textblock_render_post(Evas_Object *obj);

static int evas_object_textblock_is_opaque(Evas_Object *obj);
static int evas_object_textblock_was_opaque(Evas_Object *obj);

static void evas_object_textblock_coords_recalc(Evas_Object *obj);

static Evas_Object_Func object_func =
{
   /* methods (compulsory) */
   evas_object_textblock_free,
     evas_object_textblock_render,
     evas_object_textblock_render_pre,
     evas_object_textblock_render_post,
   /* these are optional. NULL = nothing */
     NULL,
     NULL,
     NULL,
     NULL,
     evas_object_textblock_is_opaque,
     evas_object_textblock_was_opaque,
     NULL,
     NULL,
     evas_object_textblock_coords_recalc
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
 * Adds a textblock to the given evas.
 * @param   e The given evas.
 * @return  The new textblock object.
 * @todo Find a documentation group to put this under.
 */
Evas_Object *
evas_object_textblock2_add(Evas *e)
{
   Evas_Object *obj;

   MAGIC_CHECK(e, Evas, MAGIC_EVAS);
   return NULL;
   MAGIC_CHECK_END();
   obj = evas_object_new();
   evas_object_textblock_init(obj);
   evas_object_inject(obj, e);
   return obj;
}

/* styles */
#if 0 // EXAMPLE USAGE
{
   Evas_Object *obj = my_global_object;
   Evas_Textblock_Style *ts;
   
   ts = evas_textblock2_style_new(); // create a new style
   evas_textblock2_style_set // set the style
     (ts,
      // format MUST be KEY='VALUE'[KEY='VALUE']...
      // KEY may be DEFAULT to indicate default format for a textblock before
      // any tags have been used or when no tag is active. this can never be
      // popped
      // tags starting with / (ie </blah>) will ALWAYS pop UNLESS provided
      // with an override here in the style (like /p)
      "DEFAULT='push font=Vera font_size=10 align=left color=#000000'"
      "center='push align=middle'"
      "h1='push font_size=20'"
      "red='push color=#ff0000'"
      "p='push font=Vera font_size=12 align=left'"
      "/p='pop \n\n'" // you can define end tags - override default pop on /...
      "br='\n'"
      );
   evas_object_textblock2_style_set(obj, ts); // tell object to use this style
   evas_textblock2_style_free(ts); // free style - if object no longer needs it it will also be freed. you can keep it around as long as u like though and change it and all objects using it will change too. if you dont free it it will stay around until it is freed
}
#endif

static void
_style_clear(Evas_Textblock_Style *ts)
{
   if (ts->style_text) free(ts->style_text);
   if (ts->default_tag) free(ts->default_tag);
   while (ts->tags)
     {
	Evas_Object_Style_Tag *tag;
	
	tag = (Evas_Object_Style_Tag *)ts->tags;
	ts->tags = evas_object_list_remove(ts->tags, tag);
	free(tag->tag);
	free(tag->replace);
	free(tag);
     }
   ts->style_text = NULL;
   ts->default_tag = NULL;
   ts->tags = NULL;
}

Evas_Textblock_Style *
evas_textblock2_style_new(void)
{
   Evas_Textblock_Style *ts;
   
   ts = calloc(1, sizeof(Evas_Textblock_Style));
   return ts;
}

void
evas_textblock2_style_free(Evas_Textblock_Style *ts)
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

void
evas_textblock2_style_set(Evas_Textblock_Style *ts, const char *text)
{
   Evas_List *l;
   
   if (!ts) return;
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
		  if ((*p) == '\'')
		    val_stop = p;
	       }
	     if ((key_start) && (key_stop) && (val_start) && (val_stop))
	       {
		  char *tags, *replaces;
		  Evas_Object_Style_Tag *tag;
		  
		  tags = malloc(key_stop - key_start);
		  if (tags)
		    {
		       tags[key_stop - key_start] = 0;
		       strncpy(tags, key_start, key_stop - key_start - 1);
		    }
		  
		  replaces = malloc(val_stop - val_start);
		  if (replaces)
		    {
		       replaces[val_stop - val_start] = 0;
		       strncpy(replaces, val_start, val_stop - val_start - 1);
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
				 ts->tags = evas_object_list_append(ts->tags, tag);
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
   
   for (l = ts->objects; l; l = l->next)
     {
	Evas_Object *obj;
	Evas_Object_Textblock *o;
	
	obj = l->data;
	o = (Evas_Object_Textblock *)(obj->object_data);
	evas_object_textblock2_text_markup_set(obj, o->markup_text);
     }
}

const char *
evas_textblock2_style_get(Evas_Textblock_Style *ts)
{
   if (!ts) return NULL;
   return ts->style_text;
}

/* textblock styles */
void
evas_object_textblock2_style_set(Evas_Object *obj, Evas_Textblock_Style *ts)
{
   TB_HEAD();
   if (ts == o->style) return;
   if ((ts) && (ts->delete_me)) return;
   if (o->style)
     {
	Evas_Textblock_Style *old_ts;
	
	old_ts = o->style;
	old_ts->objects = evas_list_remove(old_ts->objects, obj);
	if ((old_ts->delete_me) && (!old_ts->objects))
	  evas_textblock2_style_free(old_ts);
     }
   if (ts)
     {
	ts->objects = evas_list_append(ts->objects, ts);
	o->style = ts;
     }
   else
     {
	o->style = NULL;
     }
   evas_object_textblock2_text_markup_set(obj, o->markup_text);
}

const Evas_Textblock_Style *
evas_object_textblock2_style_get(Evas_Object *obj)
{
   TB_HEAD_RETURN(NULL);
   return o->style;
}

/* setting a textblock via markup */
#if 0 // EXAMPLE USAGE
{
   Evas *evas = my_global_evas;
   Evas_Textblock_Style *ts = my_global_style; // using previous example style
   Evas_Object *obj;
   
   obj = evas_object_textblock2_add(evas);
   evas_textblock2_style_set(obj, ts);
   evas_object_textblock2_text_markup_set
     (obj,
      "<center><h1>Title</h1></center>"
      "<p>A pragraph here <red>red text</red> and stuff.</p>"
      "<p>And escaping &lt; and &gt; as well as &amp; as normal.</p>"
      "<p>If you want a newline use <br>woo a new line!</p>"
      );
}
#endif

static char *
_style_match_replace(Evas_Textblock_Style *ts, char *s)
{
   Evas_Object_List *l;
   
   for (l = (Evas_Object_List *)ts->tags; l; l = l->next)
     {
	Evas_Object_Style_Tag *tag;
	
	tag = (Evas_Object_Style_Tag *)l;
	if (!strcmp(tag->replace, s)) return tag->tag;
     }
   return NULL;
}

static char *
_style_match_tag(Evas_Textblock_Style *ts, char *s)
{
   Evas_Object_List *l;
   
   for (l = (Evas_Object_List *)ts->tags; l; l = l->next)
     {
	Evas_Object_Style_Tag *tag;
	
	tag = (Evas_Object_Style_Tag *)l;
	if (!strcmp(tag->tag, s)) return tag->replace;
     }
   return NULL;
}

static char *
_strbuf_append(char *s, char *s2, int *len, int *alloc)
{
   int l2;
   int tlen;
   
   if (!s2) return s;
   l2 = strlen(s2);
   tlen = *len + l2;
   if (tlen > *alloc)
     {
	char *ts;
	int talloc;
	  
	talloc = ((tlen + 31) >> 5) << 5;
	ts = realloc(s, talloc);
	if (!ts) return s;
	s = ts;
	*alloc = talloc;
     }
   strcpy(s + *len, s2);
   *len = tlen;
   return s;
}

static char *
_strbuf_append_n(char *s, char *s2, int n, int *len, int *alloc)
{
   int l2;
   int tlen;
   
   if (!s2) return s;
   l2 = 0;
   if (n < 1) return s;
   else
     {
	char *p;
	for (p = s2; (l2 < n) && (*p != 0); p++, l2++);
     }
   tlen = *len + l2;
   if (tlen > *alloc)
     {
	char *ts;
	int talloc;
	  
	talloc = ((tlen + 31) >> 5) << 5;
	ts = realloc(s, talloc);
	if (!ts) return s;
	s = ts;
	*alloc = talloc;
     }
   strncpy(s + *len, s2, l2);
   *len = tlen;
   s[tlen] = 0;
   return s;
}

static void
_nodes_clear(Evas_Object *obj)
{
   Evas_Object_Textblock *o;

   o = (Evas_Object_Textblock *)(obj->object_data);
   while (o->nodes)
     {
	Evas_Object_Textblock_Node *n;
	
	n = (Evas_Object_Textblock_Node *)o->nodes;
	o->nodes = evas_object_list_remove(o->nodes, n);
	if (n->text) free(n->text);
	free(n);
     }
}

static void
_lnodes_clear(Evas_Object *obj)
{
   Evas_Object_Textblock *o;

   o = (Evas_Object_Textblock *)(obj->object_data);
   while (o->lnodes)
     {
	Evas_Object_Textblock_Lnode *ln;
	
	ln = (Evas_Object_Textblock_Lnode *)o->lnodes;
	o->lnodes = evas_object_list_remove(o->lnodes, ln);
	if (ln->text) free(ln->text);
	free(ln);
     }
}

/* table of html escapes (that i can find) this shoudl be ordered with the
 * most common first as it's a linear search to match - no hash for this
 */
static const char *_escapes[] = 
{
/* most common escaped stuff */
     "&lt;", "<",
     "&gt;", ">",
     "&amp;", "&",
     "&nbsp;", " ", /* NOTE: we will allow nbsp's to break as we map early */
     "&quot;", "\"",
     /* all the rest */
     "&copy;", "©",
     "&reg;", "®",
     "&Ntilde;", "Ñ",
     "&ntilde;", "ñ",
     "&Ccedil;", "Ç",
     "&ccedil;", "ç",
     "&szlig;", "ß",
     "&THORN;", "Þ",
     "&thorn;", "þ",
     "&ETH;", "Ð",
     "&eth;", "ð",
     "&acute;", "´",
     "&cedil;", "¸",
     "&deg;", "°",
     "&uml;", "¨",
     "&cent;", "¢",
     "&pound;", "£",
     "&curren;", "¤",
     "&yen;", "¥",
     "&sect;", "§",
     "&para;", "¶",
     "&laquo;", "«",
     "&raquo;", "»",
     "&iexcl;", "¡",
     "&iquest;", "¿",
     "&brvbar;", "¦",
     "&ordf;", "ª",
     "&ordm;", "º",
     "&micro;", "µ",
     "&macr;", "¯",
     "&oplus;", "⊕",
     "&int;", "∫",
     "&sum;", "∑",
     "&prod;", "∏",
     "&perp;", "⊥",
     "&or;", "∨",
     "&and;", "∧",
     "&equiv;", "≡",
     "&ne;", "≠",
     "&forall;", "∀",
     "&exist;", "∃",
     "&nabla;", "∇",
     "&larr;", "←",
     "&rarr;", "→",
     "&uarr;", "↑",
     "&darr;", "↓",
     "&harr;", "↔",
     "&lArr;", "⇐",
     "&rArr;", "⇒",
     "&plusmn;", "±",
     "&middot;", "·",
     "&times;", "×",
     "&divide;", "÷",
     "&sup1;", "¹",
     "&sup2;", "²",
     "&sup3;", "³",
     "&frac14;", "¼",
     "&frac12;", "½",
     "&frac34;", "¾",
     "&not;", "¬",
     "&Aacute;", "Á",
     "&Eacute;", "É",
     "&Iacute;", "Í",
     "&Oacute;", "Ó",
     "&Uacute;", "Ú",
     "&Yacute;", "Ý",
     "&aacute;", "á",
     "&eacute;", "é",
     "&iacute;", "í",
     "&oacute;", "ó",
     "&uacute;", "ú",
     "&yacute;", "ý",
     "&Acirc;", "Â",
     "&Ecirc;", "Ê",
     "&Icirc;", "Î",
     "&Ocirc;", "Ô",
     "&Ucirc;", "Û",
     "&acirc;", "â",
     "&ecirc;", "ê",
     "&icirc;", "î",
     "&ocirc;", "ô",
     "&ucirc;", "û",
     "&Agrave;", "À",
     "&Egrave;", "È",
     "&Igrave;", "Ì",
     "&Ograve;", "Ò",
     "&Ugrave;", "Ù",
     "&agrave;", "à",
     "&egrave;", "è",
     "&igrave;", "ì",
     "&ograve;", "ò",
     "&ugrave;", "ù",
     "&Auml;", "Ä",
     "&Euml;", "Ë",
     "&Iuml;", "Ï",
     "&Ouml;", "Ö",
     "&auml;", "ä",
     "&euml;", "ë",
     "&iuml;", "ï",
     "&ouml;", "ö",
     "&uuml;", "ü",
     "&yuml;", "ÿ",
     "&Atilde;", "Ã",
     "&atilde;", "ã",
     "&Otilde;", "Õ",
     "&otilde;", "õ",
     "&aring;", "å",
     "&Aring;", "Å",
     "&Oslash;", "Ø",
     "&oslash;", "ø",
     "&AElig;", "Æ",
     "&aelig;", "æ",
     "&Ntilde;", "Ñ",
     "&ntilde;", "ñ",
     "&Ccedil;", "Ç",
     "&ccedil;", "ç",
     "&szlig;", "ß",
     "&THORN;", "Þ",
     "&thorn;", "þ",
     "&ETH;", "Ð",
     "&eth;", "ð",
     "&alpha;", "α",
     "&beta;", "β",
     "&gamma;", "γ",
     "&delta;", "δ",
     "&epsilon;", "ε",
     "&zeta;", "ζ",
     "&eta;", "η",
     "&theta;", "θ",
     "&iota;", "ι",
     "&kappa;", "κ",
     "&mu;", "μ",
     "&nu;", "ν",
     "&omicron;", "ο",
     "&xi;", "ξ",
     "&pi;", "π",
     "&rho;", "ρ",
     "&sigma;", "σ",
     "&tau;", "τ",
     "&upsilon;", "υ",
     "&phi;", "φ",
     "&chi;", "χ",
     "&psi;", "ψ",
     "&omega;", "ω"
};

void
evas_object_textblock2_text_markup_set(Evas_Object *obj, const char *text)
{
   TB_HEAD();
   if (o->markup_text)
     {
	free(o->markup_text);
	o->markup_text = NULL;
     }
   if (text) o->markup_text = strdup(text);
   _nodes_clear(obj);
   _lnodes_clear(obj);
   o->changed = 1;
   evas_object_change(obj);
   if (o->markup_text)
     {
	char *s, *p;
	char *tag_start, *tag_end, *esc_start, *esc_end;
	
	tag_start = tag_end = esc_start = esc_end = NULL;
	p = o->markup_text;
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
		       
		       ttag = malloc(tag_end - tag_start);
		       if (ttag)
			 {
			    strncpy(ttag, tag_start + 1, tag_end - tag_start - 1);
			    ttag[tag_end - tag_start] = 0;
			    s = _style_match_tag(o->style, ttag);
			    if (s)
			      evas_textblock2_cursor_format_append(o->cursor, s);
			    else
			      evas_textblock2_cursor_format_append(o->cursor, ttag);
			    free(ttag);
			 }
		       tag_start = tag_end = NULL;
		    }
		  else if (esc_end)
		    {
		       int i;
		       
		       for (i = 0; i < (sizeof(_escapes) / sizeof(char *)); i += 2)
			 {
			    if (!strncmp(_escapes[i], esc_start, 
					 esc_end - esc_start + 1))
			      {
				 evas_textblock2_cursor_text_append(o->cursor, _escapes[i + 1]);
				 break;
			      }
			 }
		       esc_start = esc_end = NULL;
		    }
		  else if (s)
		    {
		       char *ts;

		       ts = malloc(p - s);
		       if (ts)
			 {
			    strncpy(ts, s, p - s - 1);
			    ts[p - s] = 0;
			    evas_textblock2_cursor_text_append(o->cursor, ts);
			    free(ts);
			 }
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

const char *
evas_object_textblock2_text_markup_get(Evas_Object *obj)
{
   TB_HEAD_RETURN(NULL);
   if (!o->markup_text)
     {
	Evas_Textblock_Cursor *cur;
	int slen = 0;
	int salloc = 0;
	
	cur = evas_object_textblock2_cursor_new(obj);
	evas_textblock2_cursor_node_first(cur);
	do
	  {
	     char *s, *p, *ps;
	     
	     s = (char *)evas_textblock2_cursor_node_text_get(cur);
	     if (s)
	       {
		  p = s;
		  ps = p;
		  for (;;)
		    {
		       if (*p == 0)
			 {
			    o->markup_text = _strbuf_append(o->markup_text,
							    ps,
							    &slen, &salloc);
			    break;
			 }
		       else if (*p == '<')
			 {
			    o->markup_text = _strbuf_append_n(o->markup_text,
							      ps, p - ps,
							      &slen, &salloc);
			    o->markup_text = _strbuf_append(o->markup_text,
							    "&lt;",
							    &slen, &salloc);
			    ps = p + 1;
			 }
		       else if (*p == '>')
			 {
			    o->markup_text = _strbuf_append_n(o->markup_text,
							      ps, p - ps,
							      &slen, &salloc);
			    o->markup_text = _strbuf_append(o->markup_text,
							    "&gt;",
							    &slen, &salloc);
			    ps = p + 1;
			 }
		       else if (*p == '&')
			 {
			    o->markup_text = _strbuf_append_n(o->markup_text,
							      ps, p - ps,
							      &slen, &salloc);
			    o->markup_text = _strbuf_append(o->markup_text,
							    "&amp;",
							    &slen, &salloc);
			    ps = p + 1;
			 }
		       /* FIXME: learn how to do all the other escapes */
		       p++;
		    }
	       }
	     else
	       {
		  s = (char *)evas_textblock2_cursor_node_format_get(cur);
		  if (s)
		    {
		       char *stag;
		       
		       o->markup_text = _strbuf_append(o->markup_text,
						       "<",
						       &slen, &salloc);
		       stag = _style_match_replace(o->style, s);
		       if (stag)
			 o->markup_text = _strbuf_append(o->markup_text,
							 stag,
							 &slen, &salloc);
		       else
			 o->markup_text = _strbuf_append(o->markup_text,
							 s,
							 &slen, &salloc);
		       o->markup_text = _strbuf_append(o->markup_text,
						       ">",
						       &slen, &salloc);
		    }
	       }
	     
	  }
	while (evas_textblock2_cursor_node_next(cur));
	evas_textblock2_cursor_free(cur);
     }
   return o->markup_text;
}

/* cursors */
const Evas_Textblock_Cursor *
evas_object_textblock2_cursor_get(Evas_Object *obj)
{
   TB_HEAD_RETURN(NULL);
   return o->cursor;
}

Evas_Textblock_Cursor *
evas_object_textblock2_cursor_new(Evas_Object *obj)
{
   Evas_Textblock_Cursor *cur;
   
   TB_HEAD_RETURN(NULL);
   cur = calloc(1, sizeof(Evas_Textblock_Cursor));
   cur->obj = obj;
   cur->node = o->nodes;
   cur->pos = 0;
   o->cursors = evas_list_append(o->cursors, cur);
   return cur;
}

void
evas_textblock2_cursor_free(Evas_Textblock_Cursor *cur)
{
   Evas_Object_Textblock *o;

   if (!cur) return;
   o = (Evas_Object_Textblock *)(cur->obj->object_data);
   if (cur == o->cursor) return;
   o->cursors = evas_list_remove(o->cursors, cur);
   free(cur);
}

void
evas_textblock2_cursor_node_first(Evas_Textblock_Cursor *cur)
{
   Evas_Object_Textblock *o;
   
   if (!cur) return;
   o = (Evas_Object_Textblock *)(cur->obj->object_data);
   cur->node = o->nodes;
   cur->pos = 0;
}

void
evas_textblock2_cursor_node_last(Evas_Textblock_Cursor *cur)
{
   Evas_Object_Textblock *o;
   
   if (!cur) return;
   o = (Evas_Object_Textblock *)(cur->obj->object_data);
   if (o->nodes)
     {
	cur->node = (Evas_Object_Textblock_Node *)(((Evas_Object_List *)(o->nodes))->last);
	if (cur->node->type == NODE_TEXT)
	  cur->pos = cur->node->len - 1;
	else
	  cur->pos = 0;
     }
   else
     {
	cur->node = NULL;
	cur->pos = 0;
     }
}

Evas_Bool
evas_textblock2_cursor_node_next(Evas_Textblock_Cursor *cur)
{
   Evas_Object_Textblock *o;
   
   if (!cur) return 0;
   o = (Evas_Object_Textblock *)(cur->obj->object_data);
   if (!cur->node) return 0;
   if (((Evas_Object_List *)(o->nodes))->next)
     {
	cur->node = (Evas_Object_Textblock_Node *)(((Evas_Object_List *)(o->nodes))->next);
	cur->pos = 0;
	return 1;
     }
   return 0;
}

Evas_Bool
evas_textblock2_cursor_node_prev(Evas_Textblock_Cursor *cur)
{
   Evas_Object_Textblock *o;
   
   if (!cur) return 0;
   o = (Evas_Object_Textblock *)(cur->obj->object_data);
   if (!cur->node) return 0;
   if (((Evas_Object_List *)(o->nodes))->prev)
     {
	cur->node = (Evas_Object_Textblock_Node *)(((Evas_Object_List *)(o->nodes))->prev);
	cur->pos = 0;
	return 1;
     }
   return 0;
}

/* text controls */
void
evas_textblock2_cursor_text_append(Evas_Textblock_Cursor *cur, const char *text)
{
   Evas_Object_Textblock *o;
   Evas_Object_Textblock_Node *n;
   
   if (!cur) return;
   o = (Evas_Object_Textblock *)(cur->obj->object_data);
   n = cur->node;
   if ((!n) || (n->type == NODE_FORMAT))
     {
	n = calloc(1, sizeof(Evas_Object_Textblock_Node));
	n->type = NODE_TEXT;
	o->nodes = evas_object_list_append(o->nodes, n);
     }
   n->text = _strbuf_append(n->text, (char *)text, &(n->len), &(n->alloc));
   cur->node = n;
   cur->pos = n->len - 1;
   _lnodes_clear(cur->obj);
   o->changed = 1;
   evas_object_change(cur->obj);
}

const char *
evas_textblock2_cursor_node_text_get(Evas_Textblock_Cursor *cur)
{
   if (!cur) return NULL;
   if (!cur->node) return NULL;
   if (cur->node->type == NODE_TEXT)
     {
	return cur->node->text;
     }
   return NULL;
}

/* formatting controls */
void
evas_textblock2_cursor_format_append(Evas_Textblock_Cursor *cur, const char *format)
{
   Evas_Object_Textblock *o;
   Evas_Object_Textblock_Node *n;
   
   if (!cur) return;
   o = (Evas_Object_Textblock *)(cur->obj->object_data);
   n = calloc(1, sizeof(Evas_Object_Textblock_Node));
   n->type = NODE_FORMAT;
   o->nodes = evas_object_list_append(o->nodes, n);
   n->text = _strbuf_append(n->text, (char *)format, &(n->len), &(n->alloc));
   cur->node = n;
   cur->pos = 0;
   _lnodes_clear(cur->obj);
   o->changed = 1;
   evas_object_change(cur->obj);
}

const char *
evas_textblock2_cursor_node_format_get(Evas_Textblock_Cursor *cur)
{
   if (!cur) return NULL;
   if (!cur->node) return NULL;
   if (cur->node->type == NODE_FORMAT)
     {
	return cur->node->text;
     }
   return NULL;
}

/* general controls */
void
evas_object_textblock2_clear(Evas_Object *obj)
{
   Evas_List *l;
   
   TB_HEAD();
   _nodes_clear(obj);
   o->cursor->node = NULL;
   o->cursor->pos = 0;
   for (l = o->cursors; l; l = l->next)
     {
	Evas_Textblock_Cursor *cur;
	
	cur = (Evas_Textblock_Cursor *)l->data;
	cur->node = NULL;
	cur->pos = 0;
     }
   if (o->markup_text)
     {
	free(o->markup_text);
	o->markup_text = NULL;
     }
   _lnodes_clear(obj);
   o->changed = 1;
   evas_object_change(obj);
}

void
evas_object_textblock2_size_requested_get(Evas_Object *obj, Evas_Coord *w, Evas_Coord *h)
{
   TB_HEAD();
   /* FIXME */
}

static void
_layout(Evas_Object *obj, int calc_only, int w, int h, int *w_ret, int *h_ret)
{
   Evas_Object_Textblock *o;
   Evas_Object_List *l;
   Evas_Object_Textblock_Lnode *lnodes = NULL;
   int x, y, wmax, hmax;
   
   x = 0;
   y = 0;
   wmax = 0;
   hmax = 0;
   o = (Evas_Object_Textblock *)(obj->object_data);
   /* FIXME: generate format stack using default format from the style */
   for (l = (Evas_Object_List *)o->nodes; l; l = l->next)
     {
	Evas_Object_Textblock_Node *n;
	
	n = (Evas_Object_Textblock_Node *)l;
	/* FIXME: go thru o->nodes and generate o->lnodes */
     }
   if (!calc_only)
     {
	o->lnodes = lnodes;
     }
}



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
   return o;
}

static void
evas_object_textblock_free(Evas_Object *obj)
{
   Evas_Object_Textblock *o;

   evas_object_textblock2_clear(obj);
   evas_object_textblock2_style_set(obj, NULL);
   o = (Evas_Object_Textblock *)(obj->object_data);
   free(o->cursor);
   while (o->cursors)
     {
	Evas_Textblock_Cursor *cur;
	
	cur = (Evas_Textblock_Cursor *)o->cursors->data;
	o->cursors = evas_list_remove_list(o->cursors, o->cursors);
	free(cur);
     }
   o->magic = 0;
   free(o);
}

static void
evas_object_textblock_render(Evas_Object *obj, void *output, void *context, void *surface, int x, int y)
{
   Evas_Object_Textblock *o;

   /* render object to surface with context, and offxet by x,y */
   o = (Evas_Object_Textblock *)(obj->object_data);
   obj->layer->evas->engine.func->context_multiplier_unset(output,
							   context);
#if 1 /* using for some debugging. will go soon */
    obj->layer->evas->engine.func->context_color_set(output,
                                                     context,
                                                     230, 160, 30, 100);
    obj->layer->evas->engine.func->rectangle_draw(output,
                                                  context,
                                                  surface,
                                                  obj->cur.cache.geometry.x + x,
                                                  obj->cur.cache.geometry.y + y,
                                                  obj->cur.cache.geometry.w,
                                                  obj->cur.cache.geometry.h);
#endif
/*
   if (o->engine_data)
     {

     }
 */
}

static void
evas_object_textblock_render_pre(Evas_Object *obj)
{
   Evas_List *updates = NULL;
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
   if (o->changed)
     {
	_layout(obj, 
		0,
		obj->cur.geometry.w, obj->cur.geometry.h,
		NULL, NULL);
	o->changed = 0;
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
	updates = evas_object_render_pre_visible_change(updates, obj, is_v, was_v);
	goto done;
     }
   /* it's not visible - we accounted for it appearing or not so just abort */
   if (!is_v) goto done;
   /* clipper changed this is in addition to anything else for obj */
   updates = evas_object_render_pre_clipper_change(updates, obj);
   /* if we restacked (layer or just within a layer) and don't clip anyone */
   if (obj->restack)
     {
	updates = evas_object_render_pre_prev_cur_add(updates, obj);
	goto done;
     }
   /* if it changed color */
   if ((obj->cur.color.r != obj->prev.color.r) ||
       (obj->cur.color.g != obj->prev.color.g) ||
       (obj->cur.color.b != obj->prev.color.b) ||
       (obj->cur.color.a != obj->prev.color.a))
     {
	updates = evas_object_render_pre_prev_cur_add(updates, obj);
	goto done;
     }
   if (o->changed)
     {
/*
	Evas_Rectangle *r;

	r = malloc(sizeof(Evas_Rectangle));
	r->x = 0; r->y = 0;
	r->w = obj->cur.geometry.w;
	r->h = obj->cur.geometry.h;
	updates = evas_list_append(updates, r);
*/
	updates = evas_object_render_pre_prev_cur_add(updates, obj);
	/* FIXME: reformat */
	o->changed = 0;
     }
   done:
   evas_object_render_pre_effect_updates(updates, obj, is_v, was_v);
}

static void
evas_object_textblock_render_post(Evas_Object *obj)
{
   Evas_Object_Textblock *o;

   /* this moves the current data to the previous state parts of the object */
   /* in whatever way is safest for the object. also if we don't need object */
   /* data anymore we can free it if the object deems this is a good idea */
   o = (Evas_Object_Textblock *)(obj->object_data);
   /* remove those pesky changes */
   while (obj->clip.changes)
     {
	Evas_Rectangle *r;

	r = (Evas_Rectangle *)obj->clip.changes->data;
	obj->clip.changes = evas_list_remove(obj->clip.changes, r);
	free(r);
     }
   /* move cur to prev safely for object data */
   obj->prev = obj->cur;
   o->prev = o->cur;
/*   o->changed = 0; */
}

static int
evas_object_textblock_is_opaque(Evas_Object *obj)
{
   Evas_Object_Textblock *o;

   /* this returns 1 if the internal object data implies that the object is */
   /* currently fulyl opque over the entire gradient it occupies */
   o = (Evas_Object_Textblock *)(obj->object_data);
   return 0;
}

static int
evas_object_textblock_was_opaque(Evas_Object *obj)
{
   Evas_Object_Textblock *o;

   /* this returns 1 if the internal object data implies that the object was */
   /* currently fulyl opque over the entire gradient it occupies */
   o = (Evas_Object_Textblock *)(obj->object_data);
   return 0;
}

static void
evas_object_textblock_coords_recalc(Evas_Object *obj)
{
   Evas_Object_Textblock *o;

   o = (Evas_Object_Textblock *)(obj->object_data);
/*   
   if ((obj->cur.geometry.w != o->last_w) ||
       (obj->cur.geometry.h != o->last_h))
     {
	o->format.dirty = 1;
	o->changed = 1;
     }
 */
}
