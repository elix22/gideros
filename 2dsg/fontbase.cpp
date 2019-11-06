#include <math.h>
#include <algorithm>
#include <stdlib.h>
#include "fontbase.h"
#include "gtexture.h"
#include <utf8.h>
#include <string.h>

#define ESC	27

FontBase::~FontBase()
{
    if (shaper_)
        delete shaper_;
}

void FontBase::chunkMetrics(struct ChunkLayout &part, float letterSpacing)
{
    getBounds(part.text.c_str(),letterSpacing,&part.x,&part.y,&part.w,&part.h);
    part.w=part.w-part.x+1;
    part.h=part.h-part.y+1;
    part.y+=part.dy;
    part.advX=getAdvanceX(part.text.c_str(),letterSpacing,-1);
    part.advY=0;
}

float FontBase::getCharIndexAtOffset(struct ChunkLayout &part, float offset, float letterSpacing)
{
	return getCharIndexAtOffset(part.text.c_str(),offset,letterSpacing,-1);
}


void FontBase::layoutHorizontal(FontBase::TextLayout *tl,int start, float w, float cw, float sw, float tabSpace, int flags,float letterSpacing, bool wrapped, int end)
{
	size_t cur=(end>=0)?end+1:tl->parts.size();
	size_t cnt=cur-start;
	float ox=0;
	bool justified=false;
	if ((flags&FontBase::TLF_JUSTIFIED)&&wrapped)
	{
        sw+=(cnt>1)?((w-cw)/(cnt-1)):0;
        justified=true;
	}
	else if (flags&FontBase::TLF_RIGHT)
		ox=w-cw;
	else if (flags&FontBase::TLF_CENTER)
		ox=(w-cw)/2;
	if (!justified) //Not justified, try to merge space separated chunks together
	{
		bool merged=false;
		for (size_t i=start;i<(cur-1);i++)
		{
			if ((tl->parts[i].sepflags&CHUNKCLASS_FLAG_BREAKABLE)&&
					(tl->parts[i].styleFlags==tl->parts[i+1].styleFlags))
			{
				tl->parts[i].text=tl->parts[i].text+" "+tl->parts[i+1].text;
				tl->parts[i].sep=tl->parts[i+1].sep;
				tl->parts.erase(tl->parts.begin()+i+1);
				cur--;
				merged=true;
                i--;
                continue;
			}
            if (merged)
			{
            	chunkMetrics(tl->parts[i],letterSpacing);
				merged=false;
			}
		}
		if (merged)
		{
			size_t i=cur-1;
        	chunkMetrics(tl->parts[i],letterSpacing);
		}
	}
	for (size_t i=start;i<cur;i++)
	{
		tl->parts[i].x+=ox;
		tl->parts[i].dx=ox;
        char sep=tl->parts[i].sep;
        float ns=(sep=='\t')?(tabSpace*(1+floor(cw/tabSpace))-cw):sw;
        if (sep==ESC) ns=0;
		ox+=tl->parts[i].advX+ns;
	}
}

FontBase::TextLayout FontBase::layoutText(const char *text, FontBase::TextLayoutParameters *params)
{
	TextLayout tl;
	float lh=getLineHeight()+params->lineSpacing;
	float sw=getAdvanceX(" ",params->letterSpacing,-1);
    float as=getAscender();
    float ds=getDescender();
    float bb=(getLineHeight()-as-ds)/2;
	bool wrap=!(params->flags&TLF_NOWRAP);
	bool breakwords=(params->flags&TLF_BREAKWORDS);
	int breaksize=0;
	if (breakwords&&params->breakchar.size())
		breaksize=getAdvanceX(params->breakchar.c_str(),params->letterSpacing,-1);
	ChunkLayout styles; //To hold styling info
	styles.styleFlags=0;
    if (params->flags&TLF_RTL)
        styles.styleFlags|=TEXTSTYLEFLAG_RTL;
    if (params->flags&TLF_LTR)
        styles.styleFlags|=TEXTSTYLEFLAG_LTR;
    if (params->flags&TLF_NOSHAPING)
        styles.styleFlags|=TEXTSTYLEFLAG_SKIPSHAPING;
	float y=0;
	float cw=0;
	int st=0;
	int lines=0;
	float tabSpace;
	if (params->tabSpace<0)
		tabSpace=-params->tabSpace;
	else
		tabSpace=params->tabSpace*sw;
	std::vector<ChunkClass> chunks;
    TextClassifier_t analyzer=(params->flags&TLF_NOBIDI)?NULL:(TextClassifier_t) g_getGlobalHook(GID_GLOBALHOOK_TEXTCLASSIFIER);
    if (analyzer) {
    	if (!analyzer(chunks,std::string(text)))
    		analyzer=NULL;
    }
    if (!analyzer)
	{
        //Cut text around spaces and control chars (ascii<' ')
    	ChunkClass cc;
        cc.script=0;
    	const char *bt=text;
    	const char *rt=bt;
    	while (*rt)
    	{
            while (((*rt)&0xFF)>' ') rt++;
            cc.sep=(*rt);
            cc.text=std::string(bt,rt-bt);
            cc.textFlags=CHUNKCLASS_FLAG_BREAKABLE;
            switch (*rt) {
        	case ' ':	cc.sepFlags=CHUNKCLASS_FLAG_BREAKABLE; break;
        	case '\n':	cc.sepFlags=CHUNKCLASS_FLAG_BREAK; break;
        	default: cc.sepFlags=0;
            }
            if (*rt)
            	rt++;
            if (cc.sep==ESC)
            {
            	//Extract escape sequence
            	if (*rt=='[') {
    				const char *ss=rt+1;
    				const char *se=ss;
    				while ((*se)&&((*se)!=']')) se++;
    				if (*se==']') {
    		            chunks.push_back(cc);
    		            rt=se+1;
    		            cc.text=std::string(ss,se-ss);
    		            cc.textFlags=CHUNKCLASS_FLAG_STYLE;
    		            cc.sep=0;
    		            cc.sepFlags=0;
    				}
            	}
            }
            bt=rt;
            chunks.push_back(cc);
    	}
	}

    uint8_t lsepflags=CHUNKCLASS_FLAG_BREAKABLE;
	for (std::vector<ChunkClass>::const_iterator it=chunks.cbegin();it!=chunks.cend();it++)
	{
		uint8_t textflags=it->textFlags;
		if (textflags&CHUNKCLASS_FLAG_STYLE) {
			const char *ss=it->text.c_str();
			const char *se=ss+it->text.size();
			while (true) {
				const char *sp=ss;
				const char *sa=NULL;
				while ((sp<se)&&((*sp)!=','))
				{
					if ((*sp)=='=') sa=sp;
					sp++;
				}
				std::string key,val;
				if (sa)
				{
					key=std::string(ss,sa-ss);
					val=std::string(sa+1,sp-sa-1);
				}
				else
					key=std::string(ss,sp-ss);
				if (!key.compare("color"))
				{
					if ((val.size()==0)||(val.at(0)!='#'))
						styles.styleFlags&=~TEXTSTYLEFLAG_COLOR;
					else
					{
						styles.styleFlags|=TEXTSTYLEFLAG_COLOR;
						int param=strtol(val.c_str()+1,NULL,16);
						switch (val.size())
						{
							case 4:
								styles.color=((param&0x0F)<<4)|((param&0x0F)<<0)|
											 ((param&0xF0)<<8)|((param&0xF0)<<4)|
											 ((param&0xF00)<<12)|((param&0xF00)<<8)|
											 0xFF000000;
								break;
							case 5:
								styles.color=((param&0x0F)<<28)|((param&0x0F)<<24)|
											 ((param&0xF0)<<0)|((param&0xF0)>>4)|
											 ((param&0xF00)<<4)|((param&0xF00)>>0)|
											 ((param&0xF000)<<8)|((param&0xF000)<<4);
								break;
							case 7:
								styles.color=param|0xFF000000;
								break;
							case 9:
								styles.color=(param>>8)|(param&0xFF)<<24;
								break;
							default:
								styles.styleFlags&=~TEXTSTYLEFLAG_COLOR;
						}
					}
				}
				if (sp==se) break;
				ss=sp+1;
			}
			continue;
		}

		ChunkLayout cl;
		cl.text=it->text;
        cl.x=cl.y=cl.w=cl.h=0;
        cl.dy=y; cl.dx=0;
		cl.sep=it->sep;
		uint8_t sepflags=it->sepFlags;
		cl.sepflags=sepflags;
		cl.line=lines+1;
		cl.styleFlags=styles.styleFlags;
		if (textflags&CHUNKCLASS_FLAG_RTL)
			cl.styleFlags=(cl.styleFlags&(~TEXTSTYLEFLAG_LTR))|TEXTSTYLEFLAG_RTL;
		if (textflags&CHUNKCLASS_FLAG_LTR)
			cl.styleFlags=(cl.styleFlags&(~TEXTSTYLEFLAG_RTL))|TEXTSTYLEFLAG_LTR;
		cl.color=styles.color;
		float ns=(cl.sep=='\t')?(tabSpace*(1+floor(cw/tabSpace))-cw):(((cl.sep==ESC)||(cl.sep==0))?0:sw);
		cl.sepl=ns;
        if (cl.text.size())
        	chunkMetrics(cl,params->letterSpacing);
        else {
            cl.advX=0;
            cl.advY=0;
            cl.shapeScaleX=0;
            cl.shapeScaleY=0;
        }
        if (wrap&&cw&&(lsepflags&CHUNKCLASS_FLAG_BREAKABLE)&&((cw+cl.w+ns)>params->w))
		{
            if (breakwords&&(cl.w>params->w)&&(cw<(params->w/2)))
            {
                // Next word is too long to fit into a line no matter what,
                // and we have still more than half a line space to fill up.
                // Don't break now for better looking
            }
            else
            {
                //The current line will exceed max width (and is not empty): wrap
                layoutHorizontal(&tl,st, params->w, cw, sw, tabSpace, params->flags,params->letterSpacing,true);
                st=tl.parts.size();
                y+=lh;
                cl.y+=lh;
                cl.dy=y;
                cw=0;
                lines++;
                cl.line=lines+1;
            }
		}
		tl.parts.push_back(cl);
        lsepflags=sepflags;
		if (cw) cw+=ns;
		cw+=cl.w;
        while (wrap&&breakwords&&(cw>params->w))
		{
			//Last line is too long but can't be cut at a space boundary: cut in as appropriate and add breakchar
			size_t pmax=tl.parts.size();
			size_t cur=st;
			float wmax=params->w-breaksize;
			float ccw=0;
			//Locate the exceeding chunk
			while ((cur<pmax)&&(wmax>(tl.parts[cur].w+tl.parts[cur].sepl))) {
				wmax-=tl.parts[cur].w+tl.parts[cur].sepl;
				ccw+=tl.parts[cur].w+tl.parts[cur].sepl;
                cur++;
            }
			if ((cur<pmax)&&(wmax>0)) //Should always happen, but better check anyhow
			{
				size_t brk=cur;
                float bsize=0;
				int cpos=getCharIndexAtOffset(tl.parts[cur],wmax,params->letterSpacing);
				if (cpos>tl.parts[cur].text.size())
					brk++;
				else if (cpos>0)
				{
					float x,y,w,h;
					cl=tl.parts[cur];
					//Cut first part
					tl.parts[cur].text=cl.text.substr(0,cpos);
					tl.parts[cur].text+=params->breakchar;
					tl.parts[cur].sepl=0;
					tl.parts[cur].sep=0;
		        	chunkMetrics(tl.parts[cur],params->letterSpacing);
		            ccw+=tl.parts[cur].w;
                    bsize=breaksize;
		            //Compute second part
					cl.text=cl.text.substr(cpos);
		        	chunkMetrics(cl,params->letterSpacing);
		            //Insert second part
					brk++;
					tl.parts.insert(tl.parts.begin()+brk,cl);
					pmax++;
				}
				if ((brk<pmax)&&(brk>st)) {
                    if (brk>st) {
                        int ln=pmax-brk;
						layoutHorizontal(&tl,st, params->w, ccw, sw, tabSpace, params->flags,params->letterSpacing,true,brk-1);
                        pmax=tl.parts.size();
                        brk=pmax-ln;
                    }
					st=brk;
					y+=lh;
					cl.y+=lh;
					cl.dy=y;
                    cw-=(ccw-bsize);
					lines++;
					cl.line=lines+1;
					for (size_t n=brk;n<pmax;n++)
					{
						tl.parts[n].line=cl.line;
						tl.parts[n].y=cl.y;
						tl.parts[n].dy=cl.dy;
					}
                    continue;
				}
			}
            break;
		}
		if (sepflags&CHUNKCLASS_FLAG_BREAK)
		{
			//Line break
			layoutHorizontal(&tl,st, params->w, cw, sw, tabSpace, params->flags,params->letterSpacing);
			st=tl.parts.size();
			y+=lh;
			cw=0;
			lines++;
		}
	}
	//Layout final line
	if (cw)
	{
		layoutHorizontal(&tl,st, params->w, cw, sw, tabSpace, params->flags,params->letterSpacing);
		st=tl.parts.size();
		y+=lh;
		cw=0;
		lines++;
	}

	//Layout lines of text
	float yo=0;
	if (params->flags&TLF_BOTTOM)
		yo=params->h-y;
	else if (params->flags&TLF_VCENTER)
		yo=(params->h-y)/2;
    if (params->flags&TLF_REF_TOP)
        yo+=as;
    else if (params->flags&TLF_REF_BOTTOM)
        yo-=ds;
    else if (params->flags&TLF_REF_MIDDLE)
        yo+=(as-ds)/2;
    else if (params->flags&TLF_REF_LINETOP)
        yo+=as+bb;
    else if (params->flags&TLF_REF_LINEBOTTOM)
        yo-=ds+bb;

	tl.x = 1e30;
	tl.y = 1e30;
	float mx=-1e30,my=-1e30;
	tl.styleFlags=0;
	for (size_t k=0;k<tl.parts.size();k++)
	{
		tl.parts[k].y+=yo;
		tl.parts[k].dy+=yo;
		tl.x=std::min(tl.x,tl.parts[k].x);
		tl.y=std::min(tl.y,tl.parts[k].y);
		mx=std::max(mx,tl.parts[k].x+tl.parts[k].w-1);
		my=std::max(my,tl.parts[k].y+tl.parts[k].h-1);
		tl.styleFlags|=tl.parts[k].styleFlags;
	}
	tl.w=mx-tl.x+1;
	tl.h=my-tl.y+1;
    tl.bh=y;
    if (tl.parts.size()==0)
    {
        tl.x=tl.y=0;
        tl.w=tl.h=0;
        tl.bh=0;
    }
	tl.lines=lines;

	return tl;
}


CompositeFont::CompositeFont(Application *application, std::vector<CompositeFontSpec> fonts) : BMFontBase(application)
{
	fonts_=fonts;
	fontInfo_.ascender = 0;
	fontInfo_.descender = 1000000;
	fontInfo_.height = 0;
	for (std::vector<CompositeFontSpec>::iterator it = fonts_.begin();
			it != fonts_.end(); it++) {
		it->font->ref();
		fontInfo_.ascender = std::max(fontInfo_.ascender,it->font->getAscender());
		fontInfo_.descender = std::min(fontInfo_.descender,it->font->getDescender());
		fontInfo_.height = std::max(fontInfo_.height,it->font->getLineHeight()-it->font->getAscender());
	}
	fontInfo_.height = fontInfo_.ascender + fontInfo_.height;
}

CompositeFont::~CompositeFont()
{
	for (std::vector<CompositeFontSpec>::iterator it = fonts_.begin();
			it != fonts_.end(); it++) {
		it->font->unref();
	}
}

void CompositeFont::drawText(std::vector<GraphicsBase> *graphicsBase, const char *text, float r, float g, float b, float a, TextLayoutParameters *layout, bool hasSample, float minx, float miny,TextLayout &l)
{
    std::vector<CompositeFontSpec>::iterator bit = fonts_.begin();
    if (bit==fonts_.end()) return;
    l=bit->font->layoutText(text, layout);
    l.styleFlags|=TEXTSTYLEFLAG_SKIPLAYOUT;
    int colorFlag=l.styleFlags&(TEXTSTYLEFLAG_COLOR);
    TextLayout l2=l;
    for (std::vector<CompositeFontSpec>::iterator it=bit;it != fonts_.end(); it++) {
        if (it!=bit) {
            for (size_t pn = 0; pn < l2.parts.size(); pn++) {
                ChunkLayout &c = l.parts[pn];
                ChunkLayout &c2 = l2.parts[pn];
                it->font->chunkMetrics(c2,layout->letterSpacing);
                c.shaped=c2.shaped;
            }
        }
    	if ((it->colorR>=0)||(it->colorG>=0)||(it->colorB>=0))
    	    l.styleFlags&=~colorFlag;
    	else
    	    l.styleFlags|=colorFlag;
        it->font->drawText(graphicsBase, text, (it->colorR<0)?r:it->colorR, (it->colorG<0)?g:it->colorG, (it->colorB<0)?b:it->colorB,(it->colorA<0)?a:it->colorA, layout, hasSample, minx-it->offsetX, miny-it->offsetY, l);
	}
}

void CompositeFont::getBounds(const char *text, float letterSpacing, float *rminx, float *rminy, float *rmaxx, float *rmaxy)
{
	float minx = 1e30;
	float miny = 1e30;
	float maxx = -1e30;
	float maxy = -1e30;
	for (std::vector<CompositeFontSpec>::iterator it = fonts_.begin();
			it != fonts_.end(); it++) {
		float pminx = 1e30;
		float pminy = 1e30;
		float pmaxx = -1e30;
		float pmaxy = -1e30;
		it->font->getBounds(text, letterSpacing, &pminx, &pminy, &pmaxx, &pmaxy);
		pminx+=it->offsetX;
		pmaxx+=it->offsetX;
		pminy+=it->offsetY;
		pmaxy+=it->offsetY;
		minx = std::min(minx, pminx);
		miny = std::min(miny, pminy);
		maxx = std::max(maxx, pmaxx);
		maxy = std::max(maxy, pmaxy);
	}
	if (rminx)
		*rminx = minx;
	if (rminy)
		*rminy = miny;
	if (rmaxx)
		*rmaxx = maxx;
	if (rmaxy)
		*rmaxy = maxy;
}

float CompositeFont::getAdvanceX(const char *text, float letterSpacing, int size)
{
	if (fonts_.empty()) return 0;
	return fonts_[0].font->getAdvanceX(text, letterSpacing, size);
}

float CompositeFont::getCharIndexAtOffset(const char *text, float offset, float letterSpacing, int size)
{
    if (fonts_.empty()) return 0;
    return fonts_[0].font->getCharIndexAtOffset(text, offset, letterSpacing, size);
}

float CompositeFont::getAscender()
{
	return fontInfo_.ascender;
}

float CompositeFont::getDescender()
{
	return fontInfo_.descender;
}

float CompositeFont::getLineHeight()
{
	return fontInfo_.height;
}
