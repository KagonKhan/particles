#ifndef YARR_EXCEPTIONS_HPP
#define YARR_EXCEPTIONS_HPP

#include <stdexcept>

#include <format>

struct GLFWInitializationError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct ImGUIInitializationError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

#endif // YARR_EXCEPTIONS_HPP