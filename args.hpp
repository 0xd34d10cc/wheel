#pragma once

#include <string>
#include <string_view>
#include <charconv>
#include <vector>


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
                std::string msg;
                msg.append("No value for flag ");
                msg.append(arg);
                return msg;
              }

              int* p = reinterpret_cast<int*>(desc.value);
              const auto val = std::string_view(argv[i]);
              auto [end, ec] =
                  std::from_chars(val.data(), val.data() + val.size(), *p);
              if (ec != std::errc() || end != (val.data() + val.size())) {
                std::string msg;
                msg.append("Value for flag ");
                msg.append(arg);
                msg.append(" is invalid. Expected integer, found ");
                msg.append(val);
                return msg;
              }
              break;
            }
            case ArgType::String: {
              ++i;
              if (i >= argc) {
                std::string msg;
                msg.append("No value for flag ");
                msg.append(arg);
                return msg;
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
        std::string msg;
        msg.append("Unknown argument: ");
        msg.append(arg);
        return msg;
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