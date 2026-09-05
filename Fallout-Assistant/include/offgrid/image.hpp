#pragma once

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <string>

namespace offgrid {

struct ImageInfo {
    std::size_t source_width{};
    std::size_t source_height{};
    int bands{};
};

struct ImageViewport {
    double center_x{0.5};
    double center_y{0.5};
    double zoom{1.0};
};

bool image_support_available();
bool ensure_pdf_page_image(
    const std::filesystem::path& pdf_path,
    const std::filesystem::path& cache_root,
    const std::string& document_id,
    std::size_t page,
    std::filesystem::path& image_path,
    std::string& error);
bool render_image_ansi(
    const std::filesystem::path& path,
    std::size_t columns,
    std::size_t rows,
    bool color,
    std::ostream& output,
    ImageInfo& info,
    std::string& error,
    const ImageViewport& viewport = {});

}  // namespace offgrid
