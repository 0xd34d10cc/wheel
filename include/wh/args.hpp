#pragma once

#include <vector>
#include <string_view>
#include <string>
#include <charconv>
#include <initializer_list>


namespace wh {

class Args {
 public:
  void flag(std::string_view flag, bool* val) {
    m_args.emplace_back(ArgDescription{flag, ArgType::Bool, val});
  }

  void integer(std::string_view flag, int* val) {
    m_args.emplace_back(ArgDescription{flag, ArgType::Int, val});
  }

  void string(std::string_view flag, std::string* val) {
    m_args.emplace_back(ArgDescription{flag, ArgType::String, val});
  }

  std::string parse(int argc, const char* argv[]) {
    const auto concat = [](std::initializer_list<std::string_view> strs) {
      size_t total = 0;
      for (const auto& s : strs) {
        total += s.size();
      }

      std::string result;
      result.reserve(total);
      for (const auto& s : strs) {
        result.append(s);
      }

      return result;
    };

    for (int i = 0; i < argc; ++i) {
      const auto arg = std::string_view(argv[i]);
      bool found = false;

      for (auto& desc : m_args) {
        if (desc.flag == arg) {
          found = true;

          switch (desc.type) {
            case ArgType::Bool: {
              bool* p = reinterpret_cast<bool*>(desc.value);
              *p = true;
              break;
            }
            case ArgType::Int: {
              ++i;
              if (i >= argc) {
                return concat({"No value for flag ", arg});
              }

              int* p = reinterpret_cast<int*>(desc.value);
              const auto val = std::string_view(argv[i]);
              auto [end, ec] =
                  std::from_chars(val.data(), val.data() + val.size(), *p);
              if (ec != std::errc() || end != (val.data() + val.size())) {
                return concat({"value for flag ", arg,
                               " is invalid (expected integer, found \"", val,
                               "\")"});
              }
              break;
            }
            case ArgType::String: {
              ++i;
              if (i >= argc) {
                return concat({"no value for flag ", arg});
              }

              std::string* p = reinterpret_cast<std::string*>(desc.value);
              *p = argv[i];
              break;
            }
          }

          break;
        }
      }

      if (!found) {
        return concat({"unknown argument: ", arg});
      }
    }

    return "";
  }

 private:
  enum class ArgType { Bool, Int, String };

  struct ArgDescription {
    std::string_view flag;
    ArgType type;
    void* value;
  };

  std::vector<ArgDescription> m_args;
};

}  // namespace wh