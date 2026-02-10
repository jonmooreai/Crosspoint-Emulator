#pragma once

class FsFile;
class Print;

/**
 * Generic image-to-BMP converter using stb_image (emulator only).
 *
 * Supports PNG, GIF, BMP, TGA, PSD, PIC, PNM — everything except JPEG
 * (which is already handled by JpegToBmpConverter / picojpeg).
 *
 * Produces the same grayscale BMP format that JpegToBmpConverter outputs
 * (2-bit with Atkinson dithering for covers, 1-bit for thumbnails).
 */
class ImageToBmpConverter {
 public:
  /// Convert an image file to a 2-bit grayscale BMP suitable for cover display.
  /// @param imageFile  Opened FsFile positioned at start of the image data.
  /// @param bmpOut     Output stream for the BMP data.
  /// @param crop       If true, crop to fill the target area; if false, fit within.
  static bool imageToBmpStream(FsFile& imageFile, Print& bmpOut, bool crop = true);

  /// Convert an image file to a 1-bit BMP at a specified thumbnail size.
  static bool imageTo1BitBmpStreamWithSize(FsFile& imageFile, Print& bmpOut,
                                           int targetMaxWidth, int targetMaxHeight);

 private:
  /// Shared internal implementation.
  static bool imageToBmpStreamInternal(FsFile& imageFile, Print& bmpOut,
                                       int targetWidth, int targetHeight,
                                       bool oneBit, bool crop);
};
