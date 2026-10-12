#include "json_support.hpp"

#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>

namespace goldbox::json_support {

bj::value read_json(const std::filesystem::path& file) {
    if (!std::filesystem::is_regular_file(file)) {
        throw std::runtime_error("JSON file is not a regular file: " + file.string());
    }
    constexpr auto max_size = 4u * 1024u * 1024u;
    if (std::filesystem::file_size(file) > max_size) {
        throw std::runtime_error("JSON file exceeds 4 MiB limit: " + file.string());
    }
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Cannot open JSON file: " + file.string());
    }
    std::string bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (bytes.size() > max_size || stream.bad()) {
        throw std::runtime_error("Cannot read JSON file safely: " + file.string());
    }
    try {
        return bj::parse(bytes);
    } catch (const std::exception& error) {
        throw std::runtime_error("Invalid JSON in " + file.string() + ": " + error.what());
    }
}

const bj::object& object(const bj::value& value, std::string_view context) {
    if (!value.is_object()) throw std::runtime_error(std::string(context) + " must be an object");
    return value.as_object();
}
const bj::array& array(const bj::value& value, std::string_view context) {
    if (!value.is_array()) throw std::runtime_error(std::string(context) + " must be an array");
    return value.as_array();
}
std::string string(const bj::value& value, std::string_view context) {
    if (!value.is_string()) throw std::runtime_error(std::string(context) + " must be a string");
    return std::string(value.as_string());
}
std::int64_t integer(const bj::value& value, std::string_view context) {
    if (value.is_int64()) return value.as_int64();
    if (value.is_uint64() && value.as_uint64() <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return static_cast<std::int64_t>(value.as_uint64());
    }
    throw std::runtime_error(std::string(context) + " must be an integer");
}
const bj::value& field(const bj::object& obj, std::string_view key, std::string_view context) {
    if (const auto* value = obj.if_contains(key)) return *value;
    throw std::runtime_error(std::string(context) + " missing required field: " + std::string(key));
}
void fields_only(const bj::object& obj, std::initializer_list<std::string_view> fields, std::string_view context) {
    for (const auto& [key, value] : obj) {
        (void)value;
        bool accepted = false;
        for (const auto& allowed : fields) {
            if (key == allowed) { accepted = true; break; }
        }
        if (!accepted) throw std::runtime_error(std::string(context) + " unexpected field: " + std::string(key));
    }
}
} // namespace goldbox::json_support
