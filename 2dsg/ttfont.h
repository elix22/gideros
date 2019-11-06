#ifndef TTFONT_H
#define TTFONT_H

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_STROKER_H

#include <fontbase.h>
#include <dib.h>
#include <wchar32.h>
#include <gstatus.h>
#include <prpath.h>
#include <map>

class TTFont : public FontBase
{
public:
    TTFont(Application *application, std::vector<FontSpec> filenames, float size, float smoothing, float outline, GStatus *status);
    virtual ~TTFont();

	virtual Type getType() const
	{
		return eTTFont;
	}

    void getBounds(const wchar32_t *text, float letterSpacing, int *pminx, int *pminy, int *pmaxx, int *pmaxy);

    Dib renderFont(const char *text, TextLayoutParameters *layout, int *pminx, int *pminy, int *pmaxx, int *pmaxy,unsigned int color, bool &isRGB,TextLayout &l);

    virtual void getBounds(const char *text, float letterSpacing, float *minx, float *miny, float *maxx, float *maxy);
    virtual float getAdvanceX(const char *text, float letterSpacing, int size = -1);
    virtual float getCharIndexAtOffset(const char *text, float offset, float letterSpacing, int size = -1);
    virtual float getAscender();
    virtual float getDescender();
    virtual float getLineHeight();
    virtual bool shapeChunk(struct ChunkLayout &part,std::vector<wchar32_t> &wtext);
    virtual void chunkMetrics(struct ChunkLayout &part, float letterSpacing);

    float getSmoothing() const
    {
        return smoothing_;
    }
    FT_Face getFace(int chr, FT_UInt &glpyhIndex);
private:
    void constructor(std::vector<FontSpec> filenames, float size, float smoothing, float outline);
    int kerning(FT_Face face, FT_UInt left, FT_UInt right) const;

private:
    struct FontFace
    {
        FT_Face face;
        FT_StreamRec stream;
        float sizeMult;
    };
    std::vector<FontFace> fontFaces_;
	int ascender_;
	int descender_;
	int height_;
    float smoothing_;
    float currentLogicalScaleX_,currentLogicalScaleY_;
    float defaultSize_;
    float outlineSize_;
    void checkLogicalScale();
    struct GlyphData
    {
    	FT_Face			face;
    	FT_UInt 		glyph;
    	int				advX;
    	int 			top;
    	int 			left;
    	unsigned int	height;
    	unsigned int    width;
    	unsigned char *	bitmap;
    	int             pitch;
    };
    std::map<wchar32_t,GlyphData> glyphCache_;
    FT_Stroker stroker;
};

#endif
