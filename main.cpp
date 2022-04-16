#include "log.hpp"
#include "args.hpp"

#include <cstdlib>
#include <chrono>


int main(int argc, const char* argv[]) {
  bool help = false;
  int n = 1;
  std::string message = "Hello, world!";

  wh::Args args;
  args.flag("--help", &help);
  args.integer("-n", &n);
  args.string("-m", &message);

  const auto err = args.parse(argc - 1, argv + 1);
  if (!err.empty()) {
    LOG_ERROR("Failed to parse arguments: {}", err);
    return EXIT_FAILURE;
  }

  if (help) {
    LOG_INFO("Usage: {} [-m <message>] [-n <number of repeats>]", argv[0]);
    return EXIT_SUCCESS;
  }

  for (int i = 0; i < n; ++i) {
    LOG_INFO("{}", message);
  }

  return EXIT_SUCCESS;
}