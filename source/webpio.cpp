// Copyright (C) 2013-2016  Niklas Rosenstein
// All rights reserved.

#include <c4d.h>
#include <c4d_apibridge.h>
#include <webp/decode.h>
#include <webp/encode.h>

#define ID_WEBP_BITMAP_SAVER    1030523
#define ID_WEBP_BITMAP_LOADER   1030524
#define BASEBITMAP_MAX_DIMENSION 16000

template <typename T, void (*FREE)(void*)> class SmartFree {

    T* ptr;

public:

    SmartFree() : ptr(nullptr) {}

    SmartFree(void* ptr) : ptr((T*) ptr) {}

    virtual ~SmartFree() {
        Free();
    }

    void Reassign(void* ptr) {
        Free();
        this->ptr = (T*) ptr;
    }

    void Free() {
        if (ptr) {
            FREE(ptr);
            ptr = nullptr;
        }
    }

    SmartFree& operator = (void* ptr) {
        Reassign(ptr);
        return *this;
    }

    T* operator () () { return ptr; }

    T* operator -> () { return ptr; }

    operator T* () { return ptr; }

    operator bool () { return ptr != nullptr; }

};

class WebPBitmapImporter : public BitmapLoaderData {

public:

    ///// BitmapLoaderData

    Bool Identify(const Filename& name, UChar* probe, Int32 size);

    IMAGERESULT Load(const Filename& name, BaseBitmap* bm, Int32 frame);

    Int32 GetSaver() { return ID_WEBP_BITMAP_SAVER; }

};

class WebPBitmapExporter : public BitmapSaverData {

public:

    ///// BitmapSaverData

    IMAGERESULT Save(const Filename& name, BaseBitmap* bm, BaseContainer* data, SAVEBIT savebits);

    /* Bool Edit(BaseContainer* data); */

    Int32 GetMaxAlphas(BaseContainer* data) { return 1; }

    Int32 GetMaxResolution(Bool layers) {
        Int32 dim = WEBP_MAX_DIMENSION;
        if (BASEBITMAP_MAX_DIMENSION < dim) dim = BASEBITMAP_MAX_DIMENSION;
        return dim;
    }

};


Bool WebPBitmapImporter::Identify(const Filename& name, UChar* probe, Int32 size) {
    return WebPGetInfo(probe, size, nullptr, nullptr) != 0;
}

IMAGERESULT WebPBitmapImporter::Load(const Filename& name, BaseBitmap* bm, Int32 frame) {
    AutoAlloc<BaseFile> file;
    if (!file->Open(name, FILEOPEN_READ, FILEDIALOG_NONE)) {
        return IMAGERESULT_NOTEXISTING;
    }

    Int64 length = file->GetLength();
    if (length < 1) {
        return IMAGERESULT_FILEERROR;
    }

    SmartFree<uint8_t, free> bytes(malloc(length));
    if (!bytes) {
        return IMAGERESULT_OUTOFMEMORY;
    }

    Int bytes_read = file->ReadBytes(bytes, length, true);

    // Retrieve the image features.
    WebPBitstreamFeatures features = {0};
    VP8StatusCode status = WebPGetFeatures(bytes, bytes_read, &features);
    switch (status) {
        case VP8_STATUS_OK:
            break;
        case VP8_STATUS_OUT_OF_MEMORY:
            return IMAGERESULT_OUTOFMEMORY;
        case VP8_STATUS_NOT_ENOUGH_DATA:
        case VP8_STATUS_BITSTREAM_ERROR:
            return IMAGERESULT_FILESTRUCTURE;
        default:
            return IMAGERESULT_MISC_ERROR;
    }

    // Decode the image.
    SmartFree<uint8_t, free> image_data;
    Int32 width, height;
    if (features.has_alpha) {
        image_data = WebPDecodeRGBA(bytes, bytes_read, &width, &height);
    }
    else {
        image_data = WebPDecodeRGB(bytes, bytes_read, &width, &height);
    }

    if (!image_data) {
        return IMAGERESULT_OUTOFMEMORY;
    }
    if (width < 1 || height < 1) {
        return IMAGERESULT_FILESTRUCTURE;
    }

    // Write the image data into the bitmap.
    Int32 depth = features.has_alpha ? 32 : 24;
    IMAGERESULT result = bm->Init(width, height, depth);
    if (result != IMAGERESULT_OK) {
        return result;
    }

    Int32 pixel_count = features.has_alpha ? 4 : 3;
    BaseBitmap* alpha_channel = nullptr;
    if (features.has_alpha) {
        alpha_channel = bm->AddChannel(true, true);
    }

    for (Int32 x=0; x < width; x++) {
        for (Int32 y=0; y < height; y++) {
            Int32 index = (y * width + x) * pixel_count;
            uint8_t* pixel = image_data() + index;

            if (alpha_channel) {
                alpha_channel->SetPixelCnt(x, y, 1, pixel + 3, alpha_channel->GetBt()/8, COLORMODE_GRAY, PIXELCNT_0);
                if (pixel[3] < 1) memset(pixel, 0, 3);
            }

            bm->SetPixel(x, y, pixel[0], pixel[1], pixel[2]);
        }
    }

    return IMAGERESULT_OK;
}

IMAGERESULT WebPBitmapExporter::Save(const Filename& name, BaseBitmap* bm, BaseContainer* data,
                                     SAVEBIT savebits) {
    Int32 width = bm->GetBw();
    Int32 height = bm->GetBh();
    if (width < 1 || height < 1) {
        return IMAGERESULT_MISC_ERROR;
    }

    Int32 maxdim = GetMaxResolution(true);
    if (width > maxdim || height > maxdim) {
        return IMAGERESULT_OUTOFMEMORY;
    }

    Int32 pixel_count = savebits & SAVEBIT_ALPHA ? 4 : 3;
    Int32 image_size = width * height * pixel_count;
    SmartFree<uint8_t, free> image_data(calloc(image_size, 1));
    if (!image_data) {
        return IMAGERESULT_OUTOFMEMORY;
    }

    BaseBitmap* alpha_channel = bm->GetInternalChannel();
    if (!Bool(savebits & SAVEBIT_ALPHA)) alpha_channel = nullptr;

    for (Int32 x=0; x < width; x++) {
        for (Int32 y=0; y < height; y++) {

            Int32 index = (y * width + x) * pixel_count;
            uint8_t* pixel = image_data() + index;

            UInt16 r, g, b;
            bm->GetPixel(x, y, &r, &g, &b);

            pixel[0] = r;
            pixel[1] = g;
            pixel[2] = b;

            if (alpha_channel) {
                UInt16 a;
                bm->GetAlphaPixel(alpha_channel, x, y, &a);
                pixel[3] = a;
            }

        }
    }

    Int32 stride = width * pixel_count;

    size_t size;
    uint8_t* output_image = nullptr;
    if (savebits & SAVEBIT_ALPHA) {
        size = WebPEncodeLosslessRGBA(image_data, width, height, stride, &output_image);
    }
    else {
        size = WebPEncodeLosslessRGB(image_data, width, height, stride, &output_image);
    }


    SmartFree<uint8_t, free> output_image_smart(output_image);

    if (size < 1 || !output_image) {
        GeDebugOut(">> Writer has not size or not mem.");
        return IMAGERESULT_MISC_ERROR;
    }

    AutoAlloc<BaseFile> file;
    if (!file->Open(name, FILEOPEN_WRITE, FILEDIALOG_NONE)) {
        return IMAGERESULT_MISC_ERROR;
    }

    IMAGERESULT result = IMAGERESULT_MISC_ERROR;
    do {
        if (!file->WriteBytes(output_image, size)) break;
        result = IMAGERESULT_OK;
    }
    while (0);

    return result;
}


Bool RegisterWebpIO() {
    RegisterBitmapLoaderPlugin(
        /* id   */ ID_WEBP_BITMAP_LOADER,
        /* name */ "WebP"_s,
        /* info */ 0,
        /* data */ new WebPBitmapImporter);

    RegisterBitmapSaverPlugin(
        /* id     */ ID_WEBP_BITMAP_SAVER,
        /* name   */ "WebP"_s,
        /* info   */ PLUGINFLAG_BITMAPSAVER_SUPPORT_8BIT | PLUGINFLAG_BITMAPSAVER_FORCESUFFIX,
        /* data   */ new WebPBitmapExporter,
        /* suffix */ "webp"_s);

    return true;
}
