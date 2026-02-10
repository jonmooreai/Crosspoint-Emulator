/**
 * ImageToBmpConverter — generic image → BMP converter for the emulator.
 *
 * Uses stb_image to decode PNG, GIF, BMP, TGA, PSD, PIC, PNM images
 * into raw pixel data, then produces the same 2-bit / 1-bit grayscale BMP
 * format that JpegToBmpConverter outputs (including Atkinson dithering).
 *
 * This file is only compiled in the emulator build (not on the ESP32).
 */

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_JPEG          // JPEG is handled by picojpeg
#define STBI_NO_HDR           // No HDR support needed
#define STBI_NO_LINEAR        // No linear light needed

#include "stb_image.h"

#include "ImageToBmpConverter.h"

#include "Arduino.h"
#include <HardwareSerial.h>
#include <SdFat.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

#include "BitmapHelpers.h"

// ============================================================================
// Target dimensions — must match JpegToBmpConverter for consistent covers
// ============================================================================
static constexpr int TARGET_MAX_WIDTH = 480;
static constexpr int TARGET_MAX_HEIGHT = 800;

// ============================================================================
// BMP writing helpers (identical to JpegToBmpConverter)
// ============================================================================
static inline void write16(Print& out, uint16_t value) {
  out.write(value & 0xFF);
  out.write((value >> 8) & 0xFF);
}

static inline void write32(Print& out, uint32_t value) {
  out.write(value & 0xFF);
  out.write((value >> 8) & 0xFF);
  out.write((value >> 16) & 0xFF);
  out.write((value >> 24) & 0xFF);
}

static inline void write32Signed(Print& out, int32_t value) {
  out.write(value & 0xFF);
  out.write((value >> 8) & 0xFF);
  out.write((value >> 16) & 0xFF);
  out.write((value >> 24) & 0xFF);
}

static void writeBmpHeader1bit(Print& bmpOut, int width, int height) {
  const int bytesPerRow = (width + 31) / 32 * 4;
  const int imageSize = bytesPerRow * height;
  const uint32_t fileSize = 62 + imageSize;

  bmpOut.write('B');
  bmpOut.write('M');
  write32(bmpOut, fileSize);
  write32(bmpOut, 0);
  write32(bmpOut, 62);

  write32(bmpOut, 40);
  write32Signed(bmpOut, width);
  write32Signed(bmpOut, -height);  // top-down
  write16(bmpOut, 1);
  write16(bmpOut, 1);
  write32(bmpOut, 0);
  write32(bmpOut, imageSize);
  write32(bmpOut, 2835);
  write32(bmpOut, 2835);
  write32(bmpOut, 2);
  write32(bmpOut, 2);

  uint8_t palette[8] = {
      0x00, 0x00, 0x00, 0x00,
      0xFF, 0xFF, 0xFF, 0x00
  };
  for (uint8_t b : palette) bmpOut.write(b);
}

static void writeBmpHeader2bit(Print& bmpOut, int width, int height) {
  const int bytesPerRow = (width * 2 + 31) / 32 * 4;
  const int imageSize = bytesPerRow * height;
  const uint32_t fileSize = 70 + imageSize;

  bmpOut.write('B');
  bmpOut.write('M');
  write32(bmpOut, fileSize);
  write32(bmpOut, 0);
  write32(bmpOut, 70);

  write32(bmpOut, 40);
  write32Signed(bmpOut, width);
  write32Signed(bmpOut, -height);  // top-down
  write16(bmpOut, 1);
  write16(bmpOut, 2);
  write32(bmpOut, 0);
  write32(bmpOut, imageSize);
  write32(bmpOut, 2835);
  write32(bmpOut, 2835);
  write32(bmpOut, 4);
  write32(bmpOut, 4);

  uint8_t palette[16] = {
      0x00, 0x00, 0x00, 0x00,
      0x55, 0x55, 0x55, 0x00,
      0xAA, 0xAA, 0xAA, 0x00,
      0xFF, 0xFF, 0xFF, 0x00
  };
  for (uint8_t b : palette) bmpOut.write(b);
}

// ============================================================================
// stb_image callback for reading from FsFile
// ============================================================================
static int stbi_fsfile_read(void* user, char* data, int size) {
  auto* file = static_cast<FsFile*>(user);
  return static_cast<int>(file->read(reinterpret_cast<uint8_t*>(data), size));
}

static void stbi_fsfile_skip(void* user, int n) {
  auto* file = static_cast<FsFile*>(user);
  file->seek(file->position() + n);
}

static int stbi_fsfile_eof(void* user) {
  auto* file = static_cast<FsFile*>(user);
  return file->position() >= file->size() ? 1 : 0;
}

// ============================================================================
// Core implementation
// ============================================================================
bool ImageToBmpConverter::imageToBmpStreamInternal(
    FsFile& imageFile, Print& bmpOut,
    int targetWidth, int targetHeight,
    bool oneBit, bool crop) {

  Serial.printf("[IMG] Decoding image via stb_image (target %dx%d, %s)\n",
                targetWidth, targetHeight, oneBit ? "1-bit" : "2-bit");

  // Set up stb_image I/O callbacks to read from FsFile
  stbi_io_callbacks callbacks;
  callbacks.read = stbi_fsfile_read;
  callbacks.skip = stbi_fsfile_skip;
  callbacks.eof  = stbi_fsfile_eof;

  int srcW = 0, srcH = 0, channels = 0;
  // Request 1 channel (grayscale) — stb_image will convert for us
  unsigned char* pixels = stbi_load_from_callbacks(&callbacks, &imageFile,
                                                   &srcW, &srcH, &channels, 1);
  if (!pixels) {
    Serial.printf("[IMG] stb_image failed: %s\n", stbi_failure_reason());
    return false;
  }

  Serial.printf("[IMG] Decoded %dx%d image (%d original channels)\n", srcW, srcH, channels);

  // Calculate output dimensions (same logic as JpegToBmpConverter)
  int outW = srcW;
  int outH = srcH;

  if (targetWidth > 0 && targetHeight > 0 &&
      (srcW > targetWidth || srcH > targetHeight)) {
    float scaleW = static_cast<float>(targetWidth) / srcW;
    float scaleH = static_cast<float>(targetHeight) / srcH;
    float scale = crop ? std::max(scaleW, scaleH)
                       : std::min(scaleW, scaleH);
    outW = std::max(1, static_cast<int>(srcW * scale));
    outH = std::max(1, static_cast<int>(srcH * scale));
    Serial.printf("[IMG] Prescaling %dx%d -> %dx%d\n", srcW, srcH, outW, outH);
  }

  // Write BMP header
  int bytesPerRow;
  if (oneBit) {
    writeBmpHeader1bit(bmpOut, outW, outH);
    bytesPerRow = (outW + 31) / 32 * 4;
  } else {
    writeBmpHeader2bit(bmpOut, outW, outH);
    bytesPerRow = (outW * 2 + 31) / 32 * 4;
  }

  // Reusable output row buffer — allocated once, cleared per row via memset.
  std::vector<uint8_t> rowBuf(bytesPerRow, 0);

  // Stack-allocate ditherers to avoid per-conversion heap allocation.
  // Only one is active based on the oneBit flag.
  Atkinson1BitDitherer ditherer1(oneBit ? outW : 1);
  AtkinsonDitherer ditherer2(oneBit ? 1 : outW);

  // Precompute fixed-point scale factors (16.16) and per-row source mapping
  // so we avoid redundant division inside the pixel loop.
  const uint32_t scaleX_fp = (static_cast<uint32_t>(srcW) << 16) / outW;
  const uint32_t scaleY_fp = (static_cast<uint32_t>(srcH) << 16) / outH;

  // Write rows top-down
  for (int outY = 0; outY < outH; outY++) {
    memset(rowBuf.data(), 0, bytesPerRow);

    // Source Y range for this output row (area averaging)
    const int srcYStart = (static_cast<uint32_t>(outY) * scaleY_fp) >> 16;
    const int srcYEnd = std::min(
        static_cast<int>((static_cast<uint32_t>(outY + 1) * scaleY_fp) >> 16), srcH);
    const int srcYCount = std::max(1, srcYEnd - srcYStart);

    for (int outX = 0; outX < outW; outX++) {
      const int srcXStart = (static_cast<uint32_t>(outX) * scaleX_fp) >> 16;
      const int srcXEnd = std::min(
          static_cast<int>((static_cast<uint32_t>(outX + 1) * scaleX_fp) >> 16), srcW);
      const int srcXCount = std::max(1, srcXEnd - srcXStart);

      // Area-average the source pixels
      int sum = 0;
      for (int sy = srcYStart; sy < srcYStart + srcYCount && sy < srcH; sy++) {
        for (int sx = srcXStart; sx < srcXStart + srcXCount && sx < srcW; sx++) {
          sum += pixels[sy * srcW + sx];
        }
      }
      const uint8_t gray = static_cast<uint8_t>(sum / (srcYCount * srcXCount));

      if (oneBit) {
        const uint8_t bit = ditherer1.processPixel(gray, outX);
        const int byteIdx = outX / 8;
        const int bitOff = 7 - (outX % 8);
        rowBuf[byteIdx] |= (bit << bitOff);
      } else {
        const uint8_t adjusted = static_cast<uint8_t>(adjustPixel(gray));
        const uint8_t twoBit = ditherer2.processPixel(adjusted, outX);
        const int byteIdx = (outX * 2) / 8;
        const int bitOff = 6 - ((outX * 2) % 8);
        rowBuf[byteIdx] |= (twoBit << bitOff);
      }
    }

    if (oneBit) ditherer1.nextRow();
    else ditherer2.nextRow();

    bmpOut.write(rowBuf.data(), bytesPerRow);

    // Yield periodically so the UI thread can run (device-fidelity / single-core simulation).
    if ((outY + 1) % 8 == 0) {
      yield();
    }
  }

  // Cleanup — only the stb_image buffer needs explicit freeing;
  // ditherers are stack-allocated and cleaned up automatically.
  stbi_image_free(pixels);

  Serial.printf("[IMG] Successfully converted image to %s BMP (%dx%d)\n",
                oneBit ? "1-bit" : "2-bit", outW, outH);
  return true;
}

// Public API — cover image (2-bit, display-size)
bool ImageToBmpConverter::imageToBmpStream(FsFile& imageFile, Print& bmpOut, bool crop) {
  return imageToBmpStreamInternal(imageFile, bmpOut,
                                  TARGET_MAX_WIDTH, TARGET_MAX_HEIGHT,
                                  false, crop);
}

// Public API — thumbnail (1-bit, custom size)
bool ImageToBmpConverter::imageTo1BitBmpStreamWithSize(
    FsFile& imageFile, Print& bmpOut,
    int targetMaxWidth, int targetMaxHeight) {
  return imageToBmpStreamInternal(imageFile, bmpOut,
                                  targetMaxWidth, targetMaxHeight,
                                  true, true);
}
