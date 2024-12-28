// Serialization implementation of canvases

#include "apps/AppPainter.h"

#include "tiffio.h"

namespace TetriumApp
{
void AppPainter::saveCanvasToFile(const std::string& filename)
{
    // Open the file
    TIFF* tiff = TIFFOpen(filename.c_str(), "w");
    if (!tiff) {
        PANIC("Failed to open file {} for writing", filename);
    }

    // Set TIFF tags for the image width, height, and sample format
    TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, _canvasWidth);
    TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, _canvasHeight);
    TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, 32);  // 32 bits per sample (float)
    TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, 4); // 4 channels (RGBA)
    TIFFSetField(tiff, TIFFTAG_ROWSPERSTRIP, _canvasHeight);

    // Set the compression type (no compression here)
    TIFFSetField(tiff, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

    float* pCanvas = reinterpret_cast<float*>(_paintSpaceBuffer.bufferAddress);

    // Write each row of pixels to the file
    for (uint32_t row = 0; row < _canvasHeight; ++row) {
        if (TIFFWriteScanline(tiff, pCanvas + row * _canvasWidth * 4, row, 0) < 0) {
            PANIC("Failed to write scanline {} to the file {}", row, filename);
        }
    }

    TIFFClose(tiff);
}

void AppPainter::loadCanvasFromFile(const std::string& filename)
{
    TIFF* tiff = TIFFOpen(filename.c_str(), "r");
    if (!tiff) {
        PANIC("Failed to open file {} for reading", filename);
    }
    // Read TIFF tags to get the image dimensions and properties
    uint32_t width, height;
    TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height);

    // Ensure the loaded image dimensions match the canvas
    if (width != _canvasWidth || height != _canvasHeight) {
        PANIC(
            "Loaded image dimensions {}x{} do not match the canvas size {}x{}",
            width,
            height,
            _canvasWidth,
            _canvasHeight
        );
    }

    float* pCanvas = reinterpret_cast<float*>(_paintSpaceBuffer.bufferAddress);

    // Read the image data into the pCanvas array
    for (uint32_t row = 0; row < _canvasHeight; ++row) {
        if (TIFFReadScanline(tiff, pCanvas + row * _canvasWidth * 4, row, 0) < 0) {
            PANIC("Failed to read scanline {} from the file {}", row, filename);
        }
    }

    TIFFClose(tiff);
    flagTexturesForUpdate();
}

} // namespace TetriumApp
