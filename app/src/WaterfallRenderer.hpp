/**
 * @file WaterfallRenderer.hpp
 * @brief OpenGL-based waterfall/spectrogram display
 */

#pragma once

#include <vector>
#include <string>
#include <cstdint>

/**
 * @brief Colormap types for waterfall display
 */
enum class WaterfallColormap {
    Viridis,        // Blue-green-yellow
    Plasma,         // Purple-red-yellow
    Inferno,        // Black-red-yellow-white
    Magma,          // Black-purple-orange-white
    GreenPhosphor,  // Classic CRT green
    BlueHot,        // Blue-white (good for weak signals)
    Grayscale       // Simple grayscale
};

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
     * @brief Render the waterfall at the specified position
     * @param x X position (screen coordinates)
     * @param y Y position (screen coordinates)
     * @param w Width to render
     * @param h Height to render
     */
    void render(float x, float y, float w, float h);

    /**
     * @brief Set the colormap by enum
     */
    void setColormap(WaterfallColormap colormap);

    /**
     * @brief Set colormap from gradient stops
     * @param stops Vector of {position, r, g, b} where position is 0-1 and RGB are 0-255
     *
     * This allows Lua to define custom colormaps without C++ changes.
     */
    void setColormapData(const std::vector<std::tuple<float, uint8_t, uint8_t, uint8_t>>& stops);

    /**
     * @brief Get current colormap
     */
    WaterfallColormap getColormap() const { return colormap_; }

    /**
     * @brief Set the dB range for mapping
     * @param minDb Minimum dB value (maps to colormap start)
     * @param maxDb Maximum dB value (maps to colormap end)
     */
    void setRange(float minDb, float maxDb);

    /**
     * @brief Get min dB
     */
    float getMinDb() const { return minDb_; }

    /**
     * @brief Get max dB
     */
    float getMaxDb() const { return maxDb_; }

    /**
     * @brief Check if initialized
     */
    bool isInitialized() const { return initialized_; }

    /**
     * @brief Get width (FFT bins)
     */
    int getWidth() const { return width_; }

    /**
     * @brief Get height (history rows)
     */
    int getHeight() const { return height_; }

    // --- Spectrum display (top portion) ---

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
    void buildColormap();
    void updateTexture();
    uint32_t dbToColor(float db);

    unsigned int textureId_ = 0;
    int width_ = 0;
    int height_ = 0;
    bool initialized_ = false;

    // Circular buffer for waterfall rows
    std::vector<std::vector<float>> rows_;
    int topRow_ = 0;  // Index of the newest row

    // Colormap
    WaterfallColormap colormap_ = WaterfallColormap::Viridis;
    std::vector<uint32_t> colormapLUT_;  // 256-entry lookup table

    // Range
    float minDb_ = -120.0f;
    float maxDb_ = -40.0f;

    // Texture data buffer (RGBA)
    std::vector<uint32_t> textureData_;
    bool textureDirty_ = true;
};
