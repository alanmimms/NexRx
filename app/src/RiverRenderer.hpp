/**
 * @file RiverRenderer.hpp
 * @brief Raylib-based River and spectral display renderer
 */

#pragma once

#include <raylib.h>
#include <vector>
#include <cstdint>
#include <tuple>

/**
 * @brief River/Spectrogram display renderer
 *
 * Uses Raylib texture streaming with a 2D circular time-frequency
 * buffer where time flows horizontally from right to left (NOW pinned on
 * the right, oldest history on the left) and frequency maps vertically
 * (lowest frequency at the bottom, highest at the top).
 */
class RiverRenderer {
public:
  RiverRenderer();
  ~RiverRenderer();

  /**
   * @brief Initialize the River display
   * @param frequencyBins Number of FFT frequency bins (vertical resolution)
   * @param historySlices Number of time history slices (horizontal resolution)
   * @return true if successful
   */
  bool init(int frequencyBins, int historySlices);

  /**
   * @brief Shutdown and free GPU resources
   */
  void shutdown();

  /**
   * @brief Add a new slice of spectrum data
   * @param data Array of power values in dB (should have 'frequencyBins' elements)
   * @param count Number of elements in data array
   */
  void addRow(const float* data, int count);

  /**
   * @brief Shift the entire waterfall horizontally (frequency bins)
   * @param bins Number of bins to shift
   */
  void horizontalShift(int bins);

  /**
   * @brief Render The River at the specified viewport
   * @param x X screen position
   * @param y Y screen position
   * @param w Width to render
   * @param h Height to render
   * @param timeSpan Relative time span to display (1.0 = full history depth)
   * @param timeOffset Relative time offset into history (0.0 = live at right edge)
   * @param freqMinNorm Normalized lower frequency boundary (0.0 = lowest bin)
   * @param freqMaxNorm Normalized upper frequency boundary (1.0 = highest bin)
   */
  void renderRiver(float x, float y, float w, float h,
                   float timeSpan = 1.0f, float timeOffset = 0.0f,
                   float freqMinNorm = 0.0f, float freqMaxNorm = 1.0f);

  /**
   * @brief Render Tektronix-style vertical phosphor live trace
   * @param data Array of power values in dB
   * @param count Number of elements in data array
   * @param x X screen position
   * @param y Y screen position
   * @param w Width of trace gutter
   * @param h Height of trace gutter
   * @param freqMinNorm Normalized lower frequency boundary
   * @param freqMaxNorm Normalized upper frequency boundary
   */
  void renderLiveTrace(const float* data, int count,
                       float x, float y, float w, float h,
                       float freqMinNorm = 0.0f, float freqMaxNorm = 1.0f);

  /**
   * @brief Legacy horizontal waterfall rendering
   */
  void render(float x, float y, float w, float h, float zoom = 1.0f, float center = 0.5f);

  /**
   * @brief Legacy horizontal spectrum analyzer rendering
   */
  void renderSpectrum(const float* data, int count, float x, float y, float w, float h);

  /**
   * @brief Set colormap from gradient stops
   * @param stops Vector of {position, r, g, b} where position is 0-1 and RGB are 0-255
   */
  void setColormapData(const std::vector<std::tuple<float, uint8_t, uint8_t, uint8_t>>& stops);

  /**
   * @brief Set the dB range for colormap mapping
   * @param minDBVal Minimum dB value
   * @param maxDBVal Maximum dB value
   */
  void setRange(float minDBVal, float maxDBVal);

  float getMinDB() const { return minDB; }
  float getMaxDB() const { return maxDB; }
  bool isInitialized() const { return initialized; }
  int getWidth() const { return freqBins; }
  int getHeight() const { return historyDepth; }

private:
  void initDefaultColormap();
  void updateRiverTexture();
  uint32_t dbToColor(float db);

  Texture2D riverTexture = { 0 };
  int freqBins = 0;
  int historyDepth = 0;
  bool initialized = false;

  // Circular buffer of historical FFT slices
  std::vector<std::vector<float>> rows;
  int topRow = 0; // Index of newest row

  // Colormap LUT (256-entry lookup table)
  std::vector<uint32_t> colormapLUT;

  // Dynamic range in dB
  float minDB = -120.0f;
  float maxDB = -40.0f;

  // Texture pixel buffer (RGBA: width = historyDepth, height = freqBins)
  std::vector<uint32_t> textureData;
  bool textureDirty = true;
};
