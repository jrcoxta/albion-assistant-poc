#include "image.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <limits>
#include <stdexcept>
namespace aa {
using Microsoft::WRL::ComPtr;
namespace {
void checked(HRESULT result) { if (FAILED(result)) throw std::runtime_error("Falha no processamento WIC da imagem"); }
ComPtr<IWICImagingFactory> factory() {
    ComPtr<IWICImagingFactory> out;
    checked(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&out)));
    return out;
}
}
Image loadImage(const std::filesystem::path& path) {
    const auto wic=factory();
    ComPtr<IWICBitmapDecoder> decoder;
    checked(wic->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder));
    ComPtr<IWICBitmapFrameDecode> frame;
    checked(decoder->GetFrame(0,&frame));
    UINT width=0,height=0;
    checked(frame->GetSize(&width,&height));
    if(!width || !height || width>16384 || height>16384) throw std::runtime_error("Dimensoes de imagem invalidas");
    ComPtr<IWICFormatConverter> converter;
    checked(wic->CreateFormatConverter(&converter));
    checked(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
    Image out{static_cast<int>(width),static_cast<int>(height),std::vector<std::uint8_t>(std::size_t(width)*height*4)};
    checked(converter->CopyPixels(nullptr,width*4,static_cast<UINT>(out.bgra.size()),out.bgra.data()));
    return out;
}
void saveImage(const Image& image, const std::filesystem::path& path) {
    if(!image.valid() || image.bgra.size()>std::numeric_limits<UINT>::max()) throw std::runtime_error("Imagem invalida");
    const auto wic=factory();
    ComPtr<IWICStream> stream;
    checked(wic->CreateStream(&stream));
    checked(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE));
    ComPtr<IWICBitmapEncoder> encoder;
    const bool bitmap=path.extension()==L".bmp" || path.extension()==L".BMP";
    checked(wic->CreateEncoder(bitmap?GUID_ContainerFormatBmp:GUID_ContainerFormatPng,nullptr,&encoder));
    checked(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache));
    ComPtr<IWICBitmapFrameEncode> frame;
    checked(encoder->CreateNewFrame(&frame,nullptr));
    checked(frame->Initialize(nullptr));
    checked(frame->SetSize(image.width,image.height));
    WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;
    checked(frame->SetPixelFormat(&format));
    ComPtr<IWICBitmap> bitmapSource;
    checked(wic->CreateBitmapFromMemory(image.width,image.height,GUID_WICPixelFormat32bppBGRA,image.width*4,
        static_cast<UINT>(image.bgra.size()),const_cast<BYTE*>(image.bgra.data()),&bitmapSource));
    checked(frame->WriteSource(bitmapSource.Get(),nullptr));
    checked(frame->Commit());
    checked(encoder->Commit());
}
}
