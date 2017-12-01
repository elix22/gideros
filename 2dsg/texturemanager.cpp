#include "texturemanager.h"
#include <gtexture.h>
#include <gimage.h>
#include <gpath.h>
#include <vector>
#include <string.h>
#include <sys/stat.h>
#include <dib.h>
#include <glog.h>
#include "ogl.h"
#include <application.h>

ShaderTexture *TextureData::id()
{
    return gtexture_getInternalTexture(gid);
}

static void append(std::vector<char>& buffer, const void *data, size_t len)
{
    size_t s = buffer.size();
    buffer.resize(s + len);
    memcpy(&buffer[s], data, len);
}

static void append(std::vector<char>& buffer, const TextureParameters& parameters)
{
    append(buffer, &parameters.filter, sizeof(parameters.filter));
    append(buffer, &parameters.wrap, sizeof(parameters.wrap));
    append(buffer, &parameters.maketransparent, sizeof(parameters.maketransparent));
    append(buffer, &parameters.transparentcolor, sizeof(parameters.transparentcolor));
    append(buffer, &parameters.grayscale, sizeof(parameters.grayscale));
}

TextureManager::TextureManager(Application* application) :
    application_(application)
{
}

TextureManager::~TextureManager()
{
}

TextureData* TextureManager::createTextureFromFile(const char* filename, const TextureParameters& parameters,bool pow2)
{
    int flags = gpath_getDriveFlags(gpath_getPathDrive(filename));

    std::vector<char> sig;
    if (flags & GPATH_RO)
    {
        append(sig, filename, strlen(filename) + 1);
        append(sig, parameters);
    }
    else
    {
        if (flags & GPATH_REAL)
        {
            struct stat s;
            stat(gpath_transform(filename), &s);

            append(sig, filename, strlen(filename) + 1);
            append(sig, parameters);
            append(sig, &s.st_mtime, sizeof(s.st_mtime));
        }
    }

    int wrap = 0;
    switch (parameters.wrap)
    {
    case eClamp:
        wrap = GTEXTURE_CLAMP;
        break;
    case eRepeat:
        wrap = GTEXTURE_REPEAT;
        break;
    }

    int filter = 0;
    switch (parameters.filter)
    {
    case eNearest:
        filter = GTEXTURE_NEAREST;
        break;
    case eLinear:
        filter = GTEXTURE_LINEAR;
        break;
    }

    int format = 0;
    int type = 0;
    switch (parameters.format)
    {
    case eRGBA8888:
        format = GTEXTURE_RGBA;
        type = GTEXTURE_UNSIGNED_BYTE;
        break;
    case eRGB888:
        format = GTEXTURE_RGB;
        type = GTEXTURE_UNSIGNED_BYTE;
        break;
    case eRGB565:
        format = GTEXTURE_RGB;
        type = GTEXTURE_UNSIGNED_SHORT_5_6_5;
        break;
    case eRGBA4444:
        format = GTEXTURE_RGBA;
        type = GTEXTURE_UNSIGNED_SHORT_4_4_4_4;
        break;
    case eRGBA5551:
        format = GTEXTURE_RGBA;
        type = GTEXTURE_UNSIGNED_SHORT_5_5_5_1;
        break;
    case eY8:
        format = GTEXTURE_LUMINANCE;
        type = GTEXTURE_UNSIGNED_BYTE;
        break;
    case eA8:
        format = GTEXTURE_ALPHA;
        type = GTEXTURE_UNSIGNED_BYTE;
        break;
    case eYA8:
        format = GTEXTURE_LUMINANCE_ALPHA;
        type = GTEXTURE_UNSIGNED_BYTE;
        break;
    }

    if (!sig.empty())
    {
        g_id gid = gtexture_reuse(format, type, wrap, filter, &sig[0], sig.size());
        if (gid != 0)
        {
            TextureData* internal = (TextureData*)gtexture_getUserData(gid);
            TextureData* data = new TextureData(*internal);
            data->gid = gid;

            return data;
        }
    }

    Dib dib(application_, filename, true, pow2, parameters.maketransparent, parameters.transparentcolor);

    if (parameters.grayscale)
        dib.convertGrayscale();

#if PREMULTIPLIED_ALPHA
    dib.premultiplyAlpha();
#endif

    g_id gid = 0;
    switch (parameters.format)
    {
    case eRGBA8888:
        gid = gtexture_create(dib.width(), dib.height(), format, type, wrap, filter, dib.data(), &sig[0], sig.size());
        break;
    case eRGB888:
    {
        std::vector<unsigned char> data = dib.to888();
        gid = gtexture_create(dib.width(), dib.height(), format, type, wrap, filter, &data[0], &sig[0], sig.size());
        break;
    }
    case eRGB565:
    {
        std::vector<unsigned short> data = dib.to565();
        gid = gtexture_create(dib.width(), dib.height(), format, type, wrap, filter, &data[0], &sig[0], sig.size());
        break;
    }
    case eRGBA4444:
    {
        std::vector<unsigned short> data = dib.to4444();
        gid = gtexture_create(dib.width(), dib.height(), format, type, wrap, filter, &data[0], &sig[0], sig.size());
        break;
    }
    case eRGBA5551:
    {
        std::vector<unsigned short> data = dib.to5551();
        gid = gtexture_create(dib.width(), dib.height(), format, type, wrap, filter, &data[0], &sig[0], sig.size());
        break;
    }
    case eY8:
    {
        std::vector<unsigned char> data = dib.toY8();
        gid = gtexture_create(dib.width(), dib.height(), format, type, wrap, filter, &data[0], NULL, 0);
        break;
    }
    case eA8:
    {
        std::vector<unsigned char> data = dib.toA8();
        gid = gtexture_create(dib.width(), dib.height(), format, type, wrap, filter, &data[0], NULL, 0);
        break;
    }
    case eYA8:
    {
        std::vector<unsigned char> data = dib.toYA8();
        gid = gtexture_create(dib.width(), dib.height(), format, type, wrap, filter, &data[0], NULL, 0);
        break;
    }
    }

    TextureData* data = new TextureData;

    data->gid = gid;
    data->parameters = parameters;
    data->width = dib.originalWidth();
    data->height = dib.originalHeight();
    data->exwidth = dib.width();
    data->exheight = dib.height();
    data->baseWidth = dib.baseOriginalWidth();
    data->baseHeight = dib.baseOriginalHeight();
    data->scale = dib.scale();

    TextureData* internal = new TextureData(*data);
    gtexture_setUserData(gid, internal);

    return data;
}

TextureData* TextureManager::createTextureFromDib(const Dib& dib, const TextureParameters& parameters)
{
    int wrap = 0;
    switch (parameters.wrap)
    {
    case eClamp:
        wrap = GTEXTURE_CLAMP;
        break;
    case eRepeat:
        wrap = GTEXTURE_REPEAT;
        break;
    }

    int filter = 0;
    switch (parameters.filter)
    {
    case eNearest:
        filter = GTEXTURE_NEAREST;
        break;
    case eLinear:
        filter = GTEXTURE_LINEAR;
        break;
    }

    int format = 0;
    int type = 0;
    switch (parameters.format)
    {
    case eRGBA8888:
        format = GTEXTURE_RGBA;
        type = GTEXTURE_UNSIGNED_BYTE;
        break;
    case eRGB888:
        format = GTEXTURE_RGB;
        type = GTEXTURE_UNSIGNED_BYTE;
        break;
    case eRGB565:
        format = GTEXTURE_RGB;
        type = GTEXTURE_UNSIGNED_SHORT_5_6_5;
        break;
    case eRGBA4444:
        format = GTEXTURE_RGBA;
        type = GTEXTURE_UNSIGNED_SHORT_4_4_4_4;
        break;
    case eRGBA5551:
        format = GTEXTURE_RGBA;
        type = GTEXTURE_UNSIGNED_SHORT_5_5_5_1;
        break;
    case eY8:
        format = GTEXTURE_LUMINANCE;
        type = GTEXTURE_UNSIGNED_BYTE;
        break;
    case eA8:
        format = GTEXTURE_ALPHA;
        type = GTEXTURE_UNSIGNED_BYTE;
        break;
    case eYA8:
        format = GTEXTURE_LUMINANCE_ALPHA;
        type = GTEXTURE_UNSIGNED_BYTE;
        break;
    }


    Dib dib2 = dib;

    if (parameters.grayscale)
        dib2.convertGrayscale();

#if PREMULTIPLIED_ALPHA
    dib2.premultiplyAlpha();
#endif


    g_id gid = 0;
    switch (parameters.format)
    {
    case eRGBA8888:
        gid = gtexture_create(dib.width(), dib.height(), format, type, wrap, filter, dib2.data(), NULL, 0);
        break;
    case eRGB888:
    {
        std::vector<unsigned char> data = dib2.to888();
        gid = gtexture_create(dib.width(), dib.height(), format, type, wrap, filter, &data[0], NULL, 0);
        break;
    }
    case eRGB565:
    {
        std::vector<unsigned short> data = dib2.to565();
        gid = gtexture_create(dib.width(), dib.height(), format, type, wrap, filter, &data[0], NULL, 0);
        break;
    }
    case eRGBA4444:
    {
        std::vector<unsigned short> data = dib2.to4444();
        gid = gtexture_create(dib.width(), dib.height(), format, type, wrap, filter, &data[0], NULL, 0);
        break;
    }
    case eRGBA5551:
    {
        std::vector<unsigned short> data = dib2.to5551();
        gid = gtexture_create(dib.width(), dib.height(), format, type, wrap, filter, &data[0], NULL, 0);
        break;
    }
    case eY8:
    {
        std::vector<unsigned char> data = dib2.toY8();
        gid = gtexture_create(dib.width(), dib.height(), format, type, wrap, filter, &data[0], NULL, 0);
        break;
    }
    case eA8:
    {
        std::vector<unsigned char> data = dib2.toA8();
        gid = gtexture_create(dib.width(), dib.height(), format, type, wrap, filter, &data[0], NULL, 0);
        break;
    }
    case eYA8:
    {
        std::vector<unsigned char> data = dib2.toYA8();
        gid = gtexture_create(dib.width(), dib.height(), format, type, wrap, filter, &data[0], NULL, 0);
        break;
    }
    }

    TextureData* data = new TextureData;

    data->gid = gid;
    data->parameters = parameters;
    data->width = dib.originalWidth();
    data->height = dib.originalHeight();
    data->exwidth = dib.width();
    data->exheight = dib.height();
    data->baseWidth = dib.baseOriginalWidth();
    data->baseHeight = dib.baseOriginalHeight();
    data->scale = dib.scale();

    return data;
}

void TextureManager::updateTextureFromDib(TextureData* data, const Dib& dib)
{

    int wrap = 0;
    switch (data->parameters.wrap)
    {
    case eClamp:
        wrap = GTEXTURE_CLAMP;
        break;
    case eRepeat:
        wrap = GTEXTURE_REPEAT;
        break;
    }

    int filter = 0;
    switch (data->parameters.filter)
    {
    case eNearest:
        filter = GTEXTURE_NEAREST;
        break;
    case eLinear:
        filter = GTEXTURE_LINEAR;
        break;
    }

    int format = 0;
    int type = 0;
    switch (data->parameters.format)
    {
    case eRGBA8888:
        format = GTEXTURE_RGBA;
        type = GTEXTURE_UNSIGNED_BYTE;
        break;
    case eRGB888:
        format = GTEXTURE_RGB;
        type = GTEXTURE_UNSIGNED_BYTE;
        break;
    case eRGB565:
        format = GTEXTURE_RGB;
        type = GTEXTURE_UNSIGNED_SHORT_5_6_5;
        break;
    case eRGBA4444:
        format = GTEXTURE_RGBA;
        type = GTEXTURE_UNSIGNED_SHORT_4_4_4_4;
        break;
    case eRGBA5551:
        format = GTEXTURE_RGBA;
        type = GTEXTURE_UNSIGNED_SHORT_5_5_5_1;
        break;
    case eY8:
        format = GTEXTURE_LUMINANCE;
        type = GTEXTURE_UNSIGNED_BYTE;
        break;
    case eA8:
        format = GTEXTURE_ALPHA;
        type = GTEXTURE_UNSIGNED_BYTE;
        break;
    case eYA8:
        format = GTEXTURE_LUMINANCE_ALPHA;
        type = GTEXTURE_UNSIGNED_BYTE;
        break;
    }

    Dib dib2 = dib;

    if (data->parameters.grayscale)
        dib2.convertGrayscale();

#if PREMULTIPLIED_ALPHA
    dib2.premultiplyAlpha();
#endif


    switch (data->parameters.format)
    {
    case eRGBA8888:
        gtexture_update(data->gid,dib.width(), dib.height(), format, type, wrap, filter, dib2.data());
        break;
    case eRGB888:
    {
        std::vector<unsigned char> datap = dib2.to888();
        gtexture_update(data->gid,dib.width(), dib.height(), format, type, wrap, filter, &datap[0]);
        break;
    }
    case eRGB565:
    {
        std::vector<unsigned short> datap = dib2.to565();
        gtexture_update(data->gid,dib.width(), dib.height(), format, type, wrap, filter, &datap[0]);
        break;
    }
    case eRGBA4444:
    {
        std::vector<unsigned short> datap = dib2.to4444();
        gtexture_update(data->gid,dib.width(), dib.height(), format, type, wrap, filter, &datap[0]);
        break;
    }
    case eRGBA5551:
    {
        std::vector<unsigned short> datap = dib2.to5551();
        gtexture_update(data->gid,dib.width(), dib.height(), format, type, wrap, filter, &datap[0]);
        break;
    }
    case eY8:
    {
        std::vector<unsigned char> datap = dib2.toY8();
        gtexture_update(data->gid,dib.width(), dib.height(), format, type, wrap, filter, &datap[0]);
        break;
    }
    case eA8:
    {
        std::vector<unsigned char> datap = dib2.toA8();
        gtexture_update(data->gid,dib.width(), dib.height(), format, type, wrap, filter, &datap[0]);
        break;
    }
    case eYA8:
    {
        std::vector<unsigned char> datap = dib2.toYA8();
        gtexture_update(data->gid,dib.width(), dib.height(), format, type, wrap, filter, &datap[0]);
        break;
    }
    }
    data->width = dib.originalWidth();
    data->height = dib.originalHeight();
    data->exwidth = dib.width();
    data->exheight = dib.height();
    data->baseWidth = dib.baseOriginalWidth();
    data->baseHeight = dib.baseOriginalHeight();
    data->scale = dib.scale();
}

static unsigned int nextpow2(unsigned int v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;

    return v;
}

TextureData* TextureManager::createRenderTarget(int w, int h, const TextureParameters& parameters)
{
    int wrap = 0;
    switch (parameters.wrap)
    {
    case eClamp:
        wrap = GTEXTURE_CLAMP;
        break;
    case eRepeat:
        wrap = GTEXTURE_REPEAT;
        break;
    }

    int filter = 0;
    switch (parameters.filter)
    {
    case eNearest:
        filter = GTEXTURE_NEAREST;
        break;
    case eLinear:
        filter = GTEXTURE_LINEAR;
        break;
    }

    int baseWidth = w;
    int baseHeight = h;
    int width = w;
    int height = h;
    int exwidth = nextpow2(w);
    int exheight = nextpow2(h);

    g_id gid = gtexture_RenderTargetCreate(exwidth, exheight, wrap, filter);

    TextureData *data = new TextureData;

    data->gid = gid;
    data->parameters = parameters;
    data->width = width;
    data->height = height;
    data->exwidth = exwidth;
    data->exheight = exheight;
    data->baseWidth = baseWidth;
    data->baseHeight = baseHeight;
    data->scale = 1.0;

    return data;
}

void TextureManager::destroyTexture(TextureData* texture)
{
    TextureData* internal = (TextureData*)gtexture_getUserData(texture->gid);
    if (gtexture_delete(texture->gid))
        if (internal)
            delete internal;
    delete texture;
}

