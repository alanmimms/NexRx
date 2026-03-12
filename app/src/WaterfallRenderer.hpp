/**
 * @file WaterfallRenderer.hpp
 * @brief OpenGL-based waterfall/spectrogram display
 */

#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <tuple>

/**
 * @brief Waterfall/spectrogram display renderer
 *
 * Uses OpenGL texture streaming with a circular buffer approach
 * for efficient scrolling without copying data each frame.
 */
class WaterfallRenderer {
public:
  WaterfallRenderer();
  ~WaterfallRenderer();

  // Non-copyable
  WaterfallRenderer(const WaterfallRenderer&) = delete;
  WaterfallRenderer& operator=(const WaterfallRenderer&) = delete;

  /**
   * @brief Initialize the waterfall display
   * @param width Number of FFT bins (horizontal resolution)
   * @param height Number of waterfall rows (scroll history)
   * @return true if successful
   */
  bool init(int width, int height);

  /**
   * @brief Shutdown and free resources
   */
  void shutdown();

  /**
   * @brief Add a new row of spectrum data
   * @param data Array of power values in dB (should have 'width' elements)
   * @param count Number of elements in data array
   */
  void addRow(const float* data, int count);

  /**
   * @brief Shift the entire waterfall horizontally
   * @param bins Number of bins to shift (positive = right, negative = left)
   */
  void horizontalShift(int bins);

  /**
   * @brief Render the waterfall at the specified position
   * @param x X position (screen coordinates)
   * @param y Y position (screen coordinates)
   * @param w Width to render
   * @param h Height to render
   */
  void render(float x, float y, float w, float h);

  /**
   * @brief Set colormap from gradient stops
   * @param stops Vector of {position, r, g, b} where position is 0-1 and RGB are 0-255
   */
  void setColormapData(const std::vector<std::tuple<float, uint8_t, uint8_t, uint8_t>>& stops);

  /**
   * @brief Set the dB range for mapping
   * @param minDB Minimum dB value (maps to colormap start)
   * @param maxDB Maximum dB value (maps to colormap end)
   */
  void setRange(float minDB, float maxDB);

  /**
   * @brief Get min dB
   */
  float getMinDB() const { return minDB; }

  /**
   * @brief Get max dB
   */
  float getMaxDB() const { return maxDB; }

  /**
   * @brief Check if initialized
   */
  bool isInitialized() const { return initialized; }

  /**
   * @brief Get width (FFT bins)
   */
  int getWidth() const { return width; }

  /**
   * @brief Get height (history rows)
   */
  int getHeight() const { return height; }

  /**
   * @brief Render spectrum analyzer above waterfall
   * @param data Current spectrum data
   * @param count Number of data points
   * @param x X position
   * @param y Y position
   * @param w Width
   * @param h Height
   */
  void renderSpectrum(const float* data, int count, float x, float y, float w, float h);

private:
  void initDefaultColormap();
  void updateTexture();
  uint32_t dbToColor(float db);

  unsigned int textureID = 0;
  int width = 0;
  int height = 0;
  bool initialized = false;

  // Circular buffer for waterfall rows
  std::vector<std::vector<float>> rows;
  int topRow = 0;  // Index of the newest row

  // Colormap LUT (256-entry lookup table)
  std::vector<uint32_t> colormapLUT;

  // Range
  float minDB = -120.0f;
  float maxDB = -40.0f;

  // Texture data buffer (RGBA)
  std::vector<uint32_t> textureData;
  bool textureDirty = true;
};
