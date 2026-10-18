// SPDX-License-Identifier: GPL-3.0-or-later
#include "view.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace goldbox {
std::vector<ChoiceLabel> layout_choice_menu(std::span<const std::string> choices, std::size_t selected,
    std::size_t columns, std::size_t rows, bool vertical) {
    if (choices.empty() || selected >= choices.size() || !columns || columns > 80 || !rows || rows > 60)
        throw std::runtime_error("Invalid menu viewport");
    std::vector<ChoiceLabel> result;
    if (vertical) {
        const auto first = selected >= rows ? selected-rows+1 : 0;
        for (auto i = first; i < choices.size() && i-first < rows; ++i)
            result.push_back({i,0,i-first,std::min(columns,choices[i].size())});
    } else {
        auto first = selected;
        auto used = std::min(columns,choices[selected].size());
        while (first && choices[first-1].size()+1 <= columns-used) {
            used += choices[--first].size()+1;
        }
        std::size_t column = 0;
        for (auto i = first; i < choices.size(); ++i) {
            const auto length = std::min(columns,choices[i].size());
            if (length > columns-column) break;
            result.push_back({i,column,0,length});
            column += length;
            if (column == columns) break;
            ++column;
        }
    }
    return result;
}
std::optional<std::size_t> choice_shortcut(std::span<const std::string> choices, unsigned char key) {
    const auto upper = [](unsigned char c) { return c >= 'a' && c <= 'z' ? c-'a'+'A' : c; };
    for (std::size_t i = 0; i < choices.size(); ++i)
        if (!choices[i].empty() && upper(choices[i].front()) == upper(key)) return i;
    return std::nullopt;
}
std::uint8_t party_name_color(const ExplorationSession& session, std::size_t member) {
    if (member >= session.party.size()) throw std::runtime_error("Party display index out of range");
    const auto address = session.party_memory ? session.party_memory->selected_address : std::nullopt;
    const std::size_t selected = address ? session.state.memory[*address] : 0;
    return selected == member ? 15 : 11;
}
namespace {
void append_page(std::deque<std::string>& lines, std::deque<std::vector<std::uint8_t>>* ink,
    std::string_view text, std::span<const std::uint8_t> colors, bool clear,
    std::size_t columns, std::size_t rows, const std::function<void()>& next_page) {
    if (!columns || columns > 80 || !rows || rows > 60)
        throw std::runtime_error("Invalid dialogue page dimensions");
    if (clear) { lines.clear(); if (ink) ink->clear(); }
    if (lines.size() > rows || std::any_of(lines.begin(),lines.end(),
        [columns](const auto& line) { return line.size() > columns; }))
        throw std::runtime_error("Dialogue page does not fit its region");
    if (ink) {
        if (ink->size() != lines.size() || colors.size() != text.size())
            throw std::runtime_error("Dialogue color state does not fit its text");
        for (std::size_t i = 0; i < lines.size(); ++i)
            if ((*ink)[i].size() != lines[i].size())
                throw std::runtime_error("Dialogue color row does not fit its text");
    }
    if (text.empty()) return;
    const auto add_line = [&] { lines.emplace_back(); if (ink) ink->emplace_back(); };
    if (lines.empty()) add_line();
    const auto newline = [&] {
        while (!lines.back().empty() && lines.back().back() == ' ') {
            lines.back().pop_back(); if (ink) ink->back().pop_back();
        }
        if (lines.size() == rows) { next_page(); lines.clear(); if (ink) ink->clear(); }
        add_line();
    };
    for (std::size_t at = 0; at < text.size();) {
        if (text[at] == '\n') { newline(); ++at; continue; }
        const bool spaces = text[at] == ' ';
        auto end = at+1;
        while (end < text.size() && text[end] != '\n' && (text[end] == ' ') == spaces) ++end;
        auto length = end-at;
        if (spaces) {
            if (!lines.back().empty()) {
                if (length > columns-lines.back().size()) newline();
                else {
                    lines.back().append(length,' ');
                    if (ink) ink->back().insert(ink->back().end(),colors.begin()+at,colors.begin()+end);
                }
            }
        } else {
            if (length <= columns && length > columns-lines.back().size()) newline();
            while (at < end) {
                if (lines.back().size() == columns) newline();
                const auto count = std::min(end-at,columns-lines.back().size());
                for (std::size_t i = 0; i < count; ++i) {
                    if (ink) ink->back().push_back(colors[at]);
                    const unsigned char ch = text[at++];
                    lines.back() += ch >= 32 && ch < 127 ? char(ch) : '?';
                }
            }
        }
        at = end;
    }
}
} // namespace
void append_dialogue(std::deque<std::string>& lines, std::string_view text, bool clear,
    std::size_t columns, std::size_t rows, const std::function<void()>& next_page) {
    append_page(lines,nullptr,text,{},clear,columns,rows,next_page);
}
void append_dialogue(DialoguePage& page, std::string_view text, bool clear,
    std::size_t columns, std::size_t rows, const std::function<void()>& next_page) {
    if (page.color > 15) throw std::runtime_error("Invalid dialogue color");
    std::string visible;
    std::vector<std::uint8_t> colors;
    auto color = page.color;
    for (std::size_t at = 0; at < text.size(); ++at) {
        if (text[at] == '%' && at+1 < text.size()) {
            const auto code = text[at+1];
            if ((code >= '0' && code <= '9') || (code >= 'A' && code <= 'F')) {
                const auto index = code <= '9' ? code-'0' : code-'A'+10;
                color = index ? index : 10;
                ++at;
                continue;
            }
            if (code == '%') ++at;
        }
        visible += text[at];
        colors.push_back(color);
    }
    append_page(page.lines,&page.ink,visible,colors,clear,columns,rows,next_page);
    page.color = color;
}
formats::IndexedImage compose_original_frame(const GameProfile::OriginalDisplay& layout,
    std::span<const formats::IndexedImage> symbols) {
    const auto width = layout.canvas[0], height = layout.canvas[1];
    if (width < 160 || width > 640 || height < 100 || height > 480 || layout.frame_runs.size() > 128)
        throw std::runtime_error("Invalid original display canvas");
    formats::IndexedImage result{width,height,std::vector<std::uint8_t>(std::size_t(width)*height,0)};
    for (const auto& r : layout.frame_runs) {
        if (r[2] >= symbols.size() || !r[3] || r[3] > 80 || r[4] > 1 ||
            r[0]+(r[4] ? 8 : r[3]*8) > width || r[1]+(r[4] ? r[3]*8 : 8) > height)
            throw std::runtime_error("Invalid original frame symbol reference");
        const auto& tile = symbols[r[2]];
        if (tile.width != 8 || tile.height != 8 || tile.pixels.size() != 64 ||
            std::any_of(tile.pixels.begin(),tile.pixels.end(),[](auto p) { return p > 15; }))
            throw std::runtime_error("Invalid original frame symbol");
        for (unsigned i = 0; i < r[3]; ++i) for (unsigned y = 0; y < 8; ++y)
            std::copy_n(tile.pixels.begin()+y*8,8,result.pixels.begin()+
                (r[1]+(r[4] ? i*8 : 0)+y)*width+r[0]+(r[4] ? 0 : i*8));
    }
    return result;
}
std::vector<formats::IndexedImage> load_startup_screens(const formats::DaxArchive& archive,
    std::span<const std::uint8_t> records) {
    if (records.empty() || records.size() > 64) throw std::runtime_error("Startup requires 1..64 title records");
    std::vector<formats::IndexedImage> screens;
    for (const auto id : records) {
        const auto decoded = formats::decode_ega_images(archive.extract(id));
        if (decoded.images.size() != 1) throw std::runtime_error("Startup title requires one EGA image per record");
        const auto& image = decoded.images.front();
        // SSI title rendering centers the image; EGA header offsets are not
        // title placement (Pool's full-width records carry a nonzero X field).
        const auto x = (320-image.width)/2, y_offset = (200-image.height)/2;
        formats::IndexedImage canvas{320,200,std::vector<std::uint8_t>(320*200,0)};
        for (std::size_t y = 0; y < image.height; ++y)
            std::copy_n(image.pixels.begin()+y*image.width,image.width,
                canvas.pixels.begin()+(y_offset+y)*320+x);
        screens.push_back(std::move(canvas));
    }
    return screens;
}
std::uint8_t font_glyph(unsigned char ascii) {
    if (ascii >= 'a' && ascii <= 'z') ascii -= 'a'-'A';
    if (ascii < 32 || ascii > 95) ascii = '?';
    return ascii >= 64 ? ascii-64 : ascii;
}
formats::IndexedImage compose_font_atlas(std::span<const formats::IndexedImage> tiles) {
    if (tiles.size() < 64 || tiles.size() > 256) throw std::runtime_error("SSI font requires 64..256 tiles");
    for (const auto& tile : tiles) {
        if (tile.width != 8 || tile.height != 8 || tile.pixels.size() != 64 ||
            std::any_of(tile.pixels.begin(),tile.pixels.end(),[](auto p) { return p != 0 && p != 15; }))
            throw std::runtime_error("SSI font requires monochrome 8x8 tiles");
    }
    formats::IndexedImage atlas{128,32,std::vector<std::uint8_t>(128*32,0)};
    for (std::size_t i = 0; i < 64; ++i)
        for (std::size_t y = 0; y < 8; ++y)
            std::copy_n(tiles[i].pixels.begin()+y*8,8,atlas.pixels.begin()+((i/16)*8+y)*128+(i%16)*8);
    return atlas;
}
formats::IndexedImage compose_wall_view(const ExplorationSession& session, const formats::WallGraphics& graphics,
    const GameProfile::ColorBackdrop* backdrop) {
    (void)session.map.cell(session.row,session.column);
    const auto direction = static_cast<unsigned>(session.facing);
    if (direction > 3) throw std::runtime_error("Invalid graphics view direction");
    formats::IndexedImage frame{88,88,std::vector<std::uint8_t>(88*88,0)};
    std::fill(frame.pixels.begin()+44*88,frame.pixels.end(),6);
    if (backdrop) {
        // Independently implemented from SSI COLOR-mode format observations;
        // research-only references are recorded in attribution/sources.json.
        constexpr std::uint8_t sky_colors[]{0,15,4,11,13,2,9,14,0,15,4,11,13,2,9,14};
        const auto indoor = session.map.cell(session.row,session.column).event_high_bit();
        const auto sky = session.state.memory[backdrop->sky_color_addresses[indoor ? 1 : 0]];
        if (sky >= std::size(sky_colors)) throw std::runtime_error("Unsupported backdrop sky color selector");
        std::fill(frame.pixels.begin(),frame.pixels.begin()+43*88,sky_colors[sky]);
        std::fill(frame.pixels.begin()+43*88,frame.pixels.begin()+44*88,15);
        std::fill(frame.pixels.begin()+44*88,frame.pixels.begin()+46*88,7);
        std::fill(frame.pixels.begin()+46*88,frame.pixels.end(),6);
    }
    constexpr int dx[]{0,1,0,-1}, dy[]{-1,0,1,0};
    const auto wall_at = [&](int forward, int lateral, unsigned side) -> unsigned {
        const int row = session.row + dy[direction]*forward + dx[direction]*lateral;
        const int col = session.column + dx[direction]*forward - dy[direction]*lateral;
        if (row < 0 || row >= 16 || col < 0 || col >= 16) return 0;
        const auto& cell = session.map.cell(row,col);
        const unsigned facing = (direction+side)%4;
        const std::uint8_t values[]{cell.north,cell.east,cell.south,cell.west};
        if (values[facing]) return values[facing];
        const int nr = row+dy[facing], nc = col+dx[facing];
        if (nr < 0 || nr >= 16 || nc < 0 || nc >= 16) return 0;
        const auto& next = session.map.cell(nr,nc);
        const std::uint8_t opposite[]{next.south,next.west,next.north,next.east};
        return opposite[facing];
    };
    const auto paint = [&](unsigned id, unsigned view, int x, int y) {
        if (!id) return;
        if (id > 15 || !graphics.present[id-1]) throw std::runtime_error("GEO wall has no selected graphics bank");
        const auto& image = graphics.walls[id-1][view];
        if (image.pixels.size() != std::size_t(image.width)*image.height)
            throw std::runtime_error("Malformed composed wall image");
        for (int row = 0; row < image.height; ++row) for (int col = 0; col < image.width; ++col) {
            const auto pixel = image.pixels[row*image.width+col];
            const int tx = x+col, ty = y+row;
            if (pixel > 15) throw std::runtime_error("Invalid wall pixel");
            if (pixel != 13 && tx >= 0 && tx < 88 && ty >= 0 && ty < 88) frame.pixels[ty*88+tx] = pixel;
        }
    };
    // Painter's order follows the three perspective planes. Dimensions are
    // format geometry, not ray-cast textures: near fronts are 56x64, then
    // 24x32 and 8x16. Side pieces bridge those planes and are already skewed.
    constexpr int spacing[]{56,24,16}, front_x[]{16,32,40}, front_y[]{8,24,32};
    constexpr int left_x[]{0,16,32}, right_x[]{72,56,48}, side_y[]{0,8,24};
    for (int depth = 2; depth >= 0; --depth) {
        const unsigned view = (2-depth)*3;
        for (int lateral = -depth-1; lateral <= depth+1; ++lateral) {
            const auto id = wall_at(depth,lateral,0);
            paint(id,view,front_x[depth]+lateral*spacing[depth],front_y[depth]);
            if (depth == 2 && wall_at(depth,lateral+1,0))
                paint(id,9,front_x[depth]+lateral*spacing[depth]+8,front_y[depth]);
        }
        for (int lateral = -depth; lateral <= 0; ++lateral)
            paint(wall_at(depth,lateral,3),view+1,left_x[depth]+lateral*spacing[depth],side_y[depth]);
        for (int lateral = 0; lateral <= depth; ++lateral)
            paint(wall_at(depth,lateral,1),view+2,right_x[depth]+lateral*spacing[depth],side_y[depth]);
    }
    return frame;
}

WallHit cast_wall(const formats::GeoMap& map, int row, int column, double dx, double dy) {
    if (row < 0 || row >= 16 || column < 0 || column >= 16 ||
        !std::isfinite(dx) || !std::isfinite(dy) || std::hypot(dx, dy) < 0.000001) {
        throw std::runtime_error("Invalid view ray");
    }
    const double infinity = std::numeric_limits<double>::infinity();
    const double step_x = dx == 0 ? infinity : 1 / std::abs(dx);
    const double step_y = dy == 0 ? infinity : 1 / std::abs(dy);
    double next_x = step_x / 2, next_y = step_y / 2;
    // A ray exits a 16x16 grid within 32 edge crossings, including corner ties.
    for (int edge = 0; edge < 33; ++edge) {
        const bool x_edge = next_x < next_y;
        const double distance = x_edge ? next_x : next_y;
        const int nr = row + (x_edge ? 0 : (dy < 0 ? -1 : 1));
        const int nc = column + (x_edge ? (dx < 0 ? -1 : 1) : 0);
        const auto& cell = map.cell(row, column);
        auto wall = x_edge ? (dx < 0 ? cell.west : cell.east) : (dy < 0 ? cell.north : cell.south);
        if (nr < 0 || nr >= 16 || nc < 0 || nc >= 16) return {distance, wall, x_edge};
        const auto& neighbor = map.cell(nr, nc);
        const auto opposite = x_edge ? (dx < 0 ? neighbor.east : neighbor.west) :
            (dy < 0 ? neighbor.south : neighbor.north);
        if (wall || opposite) return {distance, wall ? wall : opposite, x_edge};
        row = nr;
        column = nc;
        if (x_edge) next_x += step_x;
        else next_y += step_y;
    }
    throw std::runtime_error("View ray exceeded map bounds");
}
} // namespace goldbox
