#include "offgrid/image.hpp"

#include "offgrid/terminal.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#ifdef OFFGRID_HAVE_GDAL
#include <gdal_priv.h>
#endif

namespace offgrid {
namespace {

std::string safe_id(std::string id) {
    for (char& character : id) {
        if (!std::isalnum(static_cast<unsigned char>(character)) && character != '-' &&
            character != '_') character = '-';
    }
    if (id.empty()) id = "document";
    return id;
}

std::string shell_quote(const std::string& value) {
#ifdef _WIN32
    std::string quoted = "\"";
    for (const char character : value) {
        if (character == '"') quoted += "\\\"";
        else quoted += character;
    }
    return quoted + '"';
#else
    std::string quoted = "'";
    for (const char character : value) {
        if (character == '\'') quoted += "'\\''";
        else quoted += character;
    }
    return quoted + '\'';
#endif
}

int ansi256_color(
    const std::uint8_t red, const std::uint8_t green, const std::uint8_t blue) {
    const int maximum = std::max({red, green, blue});
    const int minimum = std::min({red, green, blue});
    if (maximum - minimum < 12) {
        const int value = (static_cast<int>(red) + green + blue) / 3;
        if (value < 8) return 16;
        if (value > 248) return 231;
        return 232 + std::clamp((value - 8 + 5) / 10, 0, 23);
    }
    const auto level = [](const std::uint8_t value) {
        return std::clamp((static_cast<int>(value) * 5 + 127) / 255, 0, 5);
    };
    return 16 + 36 * level(red) + 6 * level(green) + level(blue);
}

}  // namespace

bool image_support_available() {
#ifdef OFFGRID_HAVE_GDAL
    return true;
#else
    return false;
#endif
}

bool ensure_pdf_page_image(
    const std::filesystem::path& pdf_path,
    const std::filesystem::path& cache_root,
    const std::string& document_id,
    const std::size_t page,
    std::filesystem::path& image_path,
    std::string& error) {
    if (page == 0 || !std::filesystem::exists(pdf_path)) {
        error = "PDF page source is not available.";
        return false;
    }
    std::error_code filesystem_error;
    std::filesystem::create_directories(cache_root, filesystem_error);
    if (filesystem_error) {
        error = "Could not create PDF image cache: " + filesystem_error.message();
        return false;
    }
    const auto prefix = cache_root / (safe_id(document_id) + "-page-" + std::to_string(page));
    image_path = prefix;
    image_path += ".png";
    if (std::filesystem::exists(image_path)) return true;

    const std::string command =
        "pdftoppm -f " + std::to_string(page) + " -l " + std::to_string(page) +
        " -singlefile -scale-to 1400 -png " + shell_quote(pdf_path.string()) + " " +
        shell_quote(prefix.string());
    if (std::system(command.c_str()) != 0 || !std::filesystem::exists(image_path)) {
        error = "Could not rasterize the PDF page. Install Poppler/pdftoppm or use OPENPDF.";
        return false;
    }
    return true;
}

bool render_image_ansi(
    const std::filesystem::path& path,
    const std::size_t columns,
    const std::size_t rows,
    const bool color,
    std::ostream& output,
    ImageInfo& info,
    std::string& error,
    const ImageViewport& viewport) {
#ifndef OFFGRID_HAVE_GDAL
    (void)path; (void)columns; (void)rows; (void)color; (void)output; (void)info; (void)viewport;
    error = "Image support was built without GDAL.";
    return false;
#else
    if (columns < 8 || rows < 4) {
        error = "Image viewport is too small.";
        return false;
    }
    GDALAllRegister();
    auto* dataset = static_cast<GDALDataset*>(GDALOpen(path.string().c_str(), GA_ReadOnly));
    if (!dataset) {
        error = "GDAL could not open image: " + path.string();
        return false;
    }
    const int source_width = dataset->GetRasterXSize();
    const int source_height = dataset->GetRasterYSize();
    const int band_count = dataset->GetRasterCount();
    info = {static_cast<std::size_t>(source_width), static_cast<std::size_t>(source_height), band_count};
    if (band_count < 1) {
        GDALClose(dataset);
        error = "Image has no raster bands.";
        return false;
    }

    const auto zoom = std::clamp(viewport.zoom, 1.0, static_cast<double>(
        std::max(source_width, source_height)));
    const int window_width = std::max(1, static_cast<int>(std::lround(source_width / zoom)));
    const int window_height = std::max(1, static_cast<int>(std::lround(source_height / zoom)));
    const int x_offset = std::clamp(
        static_cast<int>(std::lround(std::clamp(viewport.center_x, 0.0, 1.0) * source_width -
                                     window_width / 2.0)),
        0, source_width - window_width);
    const int y_offset = std::clamp(
        static_cast<int>(std::lround(std::clamp(viewport.center_y, 0.0, 1.0) * source_height -
                                     window_height / 2.0)),
        0, source_height - window_height);

    const std::size_t pixel_rows = rows * 2;
    std::vector<std::uint8_t> red(columns * pixel_rows);
    std::vector<std::uint8_t> green(columns * pixel_rows);
    std::vector<std::uint8_t> blue(columns * pixel_rows);
    GDALRasterIOExtraArg arguments;
    INIT_RASTERIO_EXTRA_ARG(arguments);
    // Lanczos preserves substantially more edge and costume detail when a full-resolution
    // mascot or map is reduced into the terminal's two-pixels-per-cell viewport.
    arguments.eResampleAlg = GRIORA_Lanczos;
    const auto read_band = [&](const int number, std::vector<std::uint8_t>& pixels) {
        auto* band = dataset->GetRasterBand(std::min(number, band_count));
        return band && band->RasterIO(
            GF_Read, x_offset, y_offset, window_width, window_height, pixels.data(),
            static_cast<int>(columns), static_cast<int>(pixel_rows), GDT_Byte, 0, 0,
            &arguments) == CE_None;
    };
    const bool read_ok = read_band(1, red) &&
        (band_count == 1 ? (green = red, blue = red, true) :
                           (read_band(2, green) && read_band(3, blue)));
    GDALClose(dataset);
    if (!read_ok) {
        error = "GDAL failed while resampling the image.";
        return false;
    }

    constexpr std::string_view ramp = " .:-=+*#%@";
    const auto color_mode = terminal_color_mode(color);
    for (std::size_t row = 0; row < rows; ++row) {
        for (std::size_t column = 0; column < columns; ++column) {
            const auto top = row * 2 * columns + column;
            const auto bottom = top + columns;
            if (color_mode == TerminalColorMode::TrueColor) {
                output << "\033[38;2;" << static_cast<int>(red[top]) << ';'
                       << static_cast<int>(green[top]) << ';' << static_cast<int>(blue[top])
                       << "m\033[48;2;" << static_cast<int>(red[bottom]) << ';'
                       << static_cast<int>(green[bottom]) << ';' << static_cast<int>(blue[bottom])
                       << "m▀";
            } else if (color_mode == TerminalColorMode::Ansi256) {
                output << "\033[38;5;" << ansi256_color(red[top], green[top], blue[top])
                       << "m\033[48;5;"
                       << ansi256_color(red[bottom], green[bottom], blue[bottom]) << "m▀";
            } else {
                const auto luminance = static_cast<std::size_t>((
                    red[top] * 21u + green[top] * 72u + blue[top] * 7u +
                    red[bottom] * 21u + green[bottom] * 72u + blue[bottom] * 7u) / 200u);
                output << ramp[std::min(ramp.size() - 1, luminance * (ramp.size() - 1) / 255u)];
            }
        }
        if (color_mode != TerminalColorMode::Plain) output << "\033[0m";
        output << '\n';
    }
    return true;
#endif
}

}  // namespace offgrid
