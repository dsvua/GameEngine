#include "ImageLoader.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "../Renderer/Common.hpp"
#include <stdio.h>
#include <memory>

ImageData::~ImageData()
{
    if (shouldSTBFree) {
        stbi_image_free(pixels);
        stbi_image_free(hdrPixels);
    }
}

namespace util
{

struct DDS_PIXELFORMAT
{
	unsigned int dwSize;
	unsigned int dwFlags;
	unsigned int dwFourCC;
	unsigned int dwRGBBitCount;
	unsigned int dwRBitMask;
	unsigned int dwGBitMask;
	unsigned int dwBBitMask;
	unsigned int dwABitMask;
};

struct DDS_HEADER
{
	unsigned int dwSize;
	unsigned int dwFlags;
	unsigned int dwHeight;
	unsigned int dwWidth;
	unsigned int dwPitchOrLinearSize;
	unsigned int dwDepth;
	unsigned int dwMipMapCount;
	unsigned int dwReserved1[11];
	DDS_PIXELFORMAT ddspf;
	unsigned int dwCaps;
	unsigned int dwCaps2;
	unsigned int dwCaps3;
	unsigned int dwCaps4;
	unsigned int dwReserved2;
};

struct DDS_HEADER_DXT10
{
	unsigned int dxgiFormat;
	unsigned int resourceDimension;
	unsigned int miscFlag;
	unsigned int arraySize;
	unsigned int miscFlags2;
};

const unsigned int DDSCAPS2_CUBEMAP = 0x200;
const unsigned int DDSCAPS2_VOLUME = 0x200000;

const unsigned int DDS_DIMENSION_TEXTURE2D = 3;

enum DXGI_FORMAT
{
	DXGI_FORMAT_BC1_UNORM = 71,
	DXGI_FORMAT_BC1_UNORM_SRGB = 72,
	DXGI_FORMAT_BC2_UNORM = 74,
	DXGI_FORMAT_BC2_UNORM_SRGB = 75,
	DXGI_FORMAT_BC3_UNORM = 77,
	DXGI_FORMAT_BC3_UNORM_SRGB = 78,
	DXGI_FORMAT_BC4_UNORM = 80,
	DXGI_FORMAT_BC4_SNORM = 81,
	DXGI_FORMAT_BC5_UNORM = 83,
	DXGI_FORMAT_BC5_SNORM = 84,
	DXGI_FORMAT_BC6H_UF16 = 95,
	DXGI_FORMAT_BC6H_SF16 = 96,
	DXGI_FORMAT_BC7_UNORM = 98,
	DXGI_FORMAT_BC7_UNORM_SRGB = 99,
};

static unsigned int fourCC(const char (&str)[5])
{
	return (unsigned(str[0]) << 0) | (unsigned(str[1]) << 8) | (unsigned(str[2]) << 16) | (unsigned(str[3]) << 24);
}

static VkFormat getFormat(const DDS_HEADER& header, const DDS_HEADER_DXT10& header10)
{
	if (header.ddspf.dwFourCC == fourCC("DXT1"))
		return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
	if (header.ddspf.dwFourCC == fourCC("DXT3"))
		return VK_FORMAT_BC2_UNORM_BLOCK;
	if (header.ddspf.dwFourCC == fourCC("DXT5"))
		return VK_FORMAT_BC3_UNORM_BLOCK;
	if (header.ddspf.dwFourCC == fourCC("ATI1"))
		return VK_FORMAT_BC4_UNORM_BLOCK;
	if (header.ddspf.dwFourCC == fourCC("ATI2"))
		return VK_FORMAT_BC5_UNORM_BLOCK;

	if (header.ddspf.dwFourCC == fourCC("DX10"))
	{
		switch (header10.dxgiFormat)
		{
		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
			return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
			return VK_FORMAT_BC2_UNORM_BLOCK;
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
			return VK_FORMAT_BC3_UNORM_BLOCK;
		case DXGI_FORMAT_BC4_UNORM:
			return VK_FORMAT_BC4_UNORM_BLOCK;
		case DXGI_FORMAT_BC4_SNORM:
			return VK_FORMAT_BC4_SNORM_BLOCK;
		case DXGI_FORMAT_BC5_UNORM:
			return VK_FORMAT_BC5_UNORM_BLOCK;
		case DXGI_FORMAT_BC5_SNORM:
			return VK_FORMAT_BC5_SNORM_BLOCK;
		case DXGI_FORMAT_BC6H_UF16:
			return VK_FORMAT_BC6H_UFLOAT_BLOCK;
		case DXGI_FORMAT_BC6H_SF16:
			return VK_FORMAT_BC6H_SFLOAT_BLOCK;
		case DXGI_FORMAT_BC7_UNORM:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
			return VK_FORMAT_BC7_UNORM_BLOCK;
		}
	}

	return VK_FORMAT_UNDEFINED;
}

static size_t getImageSizeBC(unsigned int width, unsigned int height, unsigned int levels, unsigned int blockSize)
{
	size_t result = 0;

	for (unsigned int i = 0; i < levels; ++i)
	{
		result += ((width + 3) / 4) * ((height + 3) / 4) * blockSize;

		width = width > 1 ? width / 2 : 1;
		height = height > 1 ? height / 2 : 1;
	}

	return result;
}

ImageData loadDDSImage(const std::filesystem::path& p)
{
    ImageData data;
	FILE* file = fopen(p.c_str(), "rb");
	if (!file)
		return data;

	std::unique_ptr<FILE, int (*)(FILE*)> filePtr(file, fclose);

	unsigned int magic = 0;
	if (fread(&magic, sizeof(magic), 1, file) != 1 || magic != fourCC("DDS "))
		return data;

	DDS_HEADER header = {};
	if (fread(&header, sizeof(header), 1, file) != 1)
		return data;

	DDS_HEADER_DXT10 header10 = {};
	if (header.ddspf.dwFourCC == fourCC("DX10") && fread(&header10, sizeof(header10), 1, file) != 1)
		return data;

	if (header.dwSize != sizeof(header) || header.ddspf.dwSize != sizeof(header.ddspf))
		return data;

	if (header.dwCaps2 & (DDSCAPS2_CUBEMAP | DDSCAPS2_VOLUME))
		return data;

	if (header.ddspf.dwFourCC == fourCC("DX10") && header10.resourceDimension != DDS_DIMENSION_TEXTURE2D)
		return data;

	VkFormat format = getFormat(header, header10);
    data.format = format;

	if (format == VK_FORMAT_UNDEFINED)
		return data;

	unsigned int blockSize =
	    (format == VK_FORMAT_BC1_RGBA_UNORM_BLOCK || format == VK_FORMAT_BC4_SNORM_BLOCK || format == VK_FORMAT_BC4_UNORM_BLOCK) ? 8 : 16;
	size_t imageSize = getImageSizeBC(header.dwWidth, header.dwHeight, 1, blockSize);

    data.pixels = (unsigned char *)malloc(imageSize);

	size_t readSize = fread(data.pixels, 1, imageSize, file);
	if (readSize != imageSize)
    {
        free(data.pixels);
        data.pixels = nullptr;
        return data;
    }

	// if (fgetc(file) != -1)
    // {
    //     free(data.pixels);
    //     data.pixels = nullptr;
    //     return data;
    // }

	filePtr.reset();
	file = nullptr;

	return data;
}

ImageData loadSTBImage(const std::filesystem::path& p)
{
    ImageData data;
    data.shouldSTBFree = true;
    if (stbi_is_hdr(p.string().c_str())) {
        data.hdr = true;
        data.hdrPixels = stbi_loadf(p.string().c_str(), &data.width, &data.height, &data.comp, 4);
    } else {
        data.pixels = stbi_load(p.string().c_str(), &data.width, &data.height, &data.channels, 4);
    }
    data.channels = 4;
    return data;
}

ImageData loadImage(const std::filesystem::path& p)
{
    auto ext = p.extension();

    if(ext == "dds" || ext == "DDS")
        return loadDDSImage(p);

    return loadSTBImage(p);
}
} // namespace util
