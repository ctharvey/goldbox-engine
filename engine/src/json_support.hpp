#pragma once
#include <boost/json.hpp>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>

namespace goldbox::json_support {
namespace bj = boost::json;

[[nodiscard]] bj::value read_json(const std::filesystem::path& file);
[[nodiscard]] const bj::object& object(const bj::value& value, std::string_view context);
[[nodiscard]] const bj::array& array(const bj::value& value, std::string_view context);
[[nodiscard]] std::string string(const bj::value& value, std::string_view context);
[[nodiscard]] std::int64_t integer(const bj::value& value, std::string_view context);
[[nodiscard]] const bj::value& field(const bj::object& object, std::string_view key, std::string_view context);
void fields_only(const bj::object& object, std::initializer_list<std::string_view> fields, std::string_view context);

} // namespace goldbox::json_support
