#include "log.hpp"

#include <cstdlib>
#include <chrono>


int main() {
  LOG_INFO("The number is {}, kek: {}, time: {}", 42,
           std::string_view("kekus maximus"), std::chrono::seconds(722));
  return EXIT_SUCCESS;
}