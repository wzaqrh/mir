#pragma once
#include <ft2build.h>
#include <freetype.h>
#include <freetype2/freetype/ftglyph.h>
#include "core/predeclare.h"
#include "core/base/math.h"

namespace mir {

struct FontTextureAtlas 
{
	MIR_MAKE_ALIGNED_OPERATOR_NEW;
	FontTextureAtlas(ITexturePtr texture, Eigen::Vector2f uv0, Eigen::Vector2f uv1, Eigen::Vector2i pos0, Eigen::Vector2i pos1) 
		: Texture(texture), Uv0(uv0), Uv1(uv1), Pos0(pos0), Pos1(pos1) {}
public:
	ITexturePtr Texture;
	Eigen::Vector2f Uv0, Uv1;
	Eigen::Vector2i Pos0, Pos1;
};
typedef std::shared_ptr<FontTextureAtlas> FontTextureAtlasPtr;

class FontTexture 
{
public:
	MIR_MAKE_ALIGNED_OPERATOR_NEW;
	FontTexture(ResourceManager& resourceMng, Eigen::Vector2i size);

	FontTextureAtlasPtr GetCharactor(int ch);
	bool ContainsCharactor(int ch) const;
	FontTextureAtlasPtr AddCharactor(int ch, int w, int h, unsigned char* buffer);
public:
	ITexturePtr GetTexture();
	int Width() const { return mWidth; }
	int Height() const { return mHeight; }
private:
	ResourceManager& mResourceMng;
	int mWidth = 0, mHeight = 0;
	ITexturePtr mTexture;
	std::vector<char> mRawBuffer;
	bool mRawBufferDirty = false;

	std::map<int, FontTextureAtlasPtr> mAtlasByCh;
	int mCol = 0, mRow = 0, mColH = 0;
};
typedef std::shared_ptr<FontTexture> FontTexturePtr;

struct FontCharactor
{
	MIR_MAKE_ALIGNED_OPERATOR_NEW;

	FT_UInt CharIndex;
	FT_Glyph Glyph;
	FT_BBox BBox;	//dot space

	Eigen::Vector2i Size;	//dot space   
	Eigen::Vector2i Bearing;	//dot space
	int Advance;	//dot space
	FontTextureAtlasPtr Atlas;
};
typedef std::shared_ptr<FontCharactor> FontCharactorPtr;

class FontCharactorCache 
{
public:
	MIR_MAKE_ALIGNED_OPERATOR_NEW;
	FontCharactorCache(ResourceManager& resourceMng, FT_Face ftFace);
	FontCharactorPtr GetCharactor(int ch);
	void FlushChange();
private:
	FontTexturePtr AllocFontTexture();
private:
	ResourceManager& mResourceMng;
	FT_Face mFtFace;
	std::vector<FontTexturePtr> mFontTextures;
	FontTexturePtr mCurFontTexture;
	std::map<int, FontCharactorPtr> mCharactors;
};
typedef std::shared_ptr<FontCharactorCache> FontCharactorCachePtr;

class Font 
{
public:
	MIR_MAKE_ALIGNED_OPERATOR_NEW;
	Font(ResourceManager& resourceMng, FT_Library ftLib, std::string fontPath, int fontSize, int dpi = 72);
	~Font();
public:
	FT_Face GetFtFace() { return mFtFace; }
	FontCharactorPtr GetCharactor(int ch);
	void Flush();
private:
	std::string mFontName;
	int mFontSize;
	FontCharactorCachePtr mCharactorCache;
	FT_Face mFtFace;
};
typedef std::shared_ptr<Font> FontPtr;

class FontCache
{
public:
	MIR_MAKE_ALIGNED_OPERATOR_NEW;
	FontCache(ResourceManager& resourceMng, int dpi = 72);
	~FontCache();
	FontPtr GetFont(std::string fontPath, int fontSize);
private:
	ResourceManager& mResourceMng;
	FT_Library mFtLib;
	int mDPI;
	struct FontKey {
		std::string path;
		int size;
		bool operator<(const FontKey& other) const {
			if (path != other.path) return path < other.path;
			return size < other.size;
		}
	};
	std::map<FontKey, FontPtr> mFonts;
};
typedef std::shared_ptr<FontCache> FontCachePtr;

}