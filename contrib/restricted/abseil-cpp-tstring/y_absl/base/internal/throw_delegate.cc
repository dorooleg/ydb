// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "y_absl/base/internal/throw_delegate.h"

#include <cstdlib>
#include <functional>
#include <new>
#include <stdexcept>

#include "y_absl/base/config.h"
#include "y_absl/base/internal/raw_logging.h"

namespace y_absl {
Y_ABSL_NAMESPACE_BEGIN
namespace base_internal {

// NOTE: The various STL exception throwing functions are placed within the
// #ifdef blocks so the symbols aren't exposed on platforms that don't support
// them, such as the Android NDK. For example, ANGLE fails to link when building
// within AOSP without them, since the STL functions don't exist.
namespace {
#ifdef Y_ABSL_HAVE_EXCEPTIONS
template <typename T>
[[noreturn]] void Throw(const T& error) {
  throw error;
}
#endif
}  // namespace

void ThrowStdLogicError(const TString& what_arg) {
#ifdef Y_ABSL_HAVE_EXCEPTIONS
  Throw(std::logic_error(what_arg));
#else
  Y_ABSL_RAW_LOG(FATAL, "%s", what_arg.c_str());
  std::abort();
#endif
}
void ThrowStdLogicError(const char* what_arg) {
#ifdef Y_ABSL_HAVE_EXCEPTIONS
  Throw(std::logic_error(what_arg));
#else
  Y_ABSL_RAW_LOG(FATAL, "%s", what_arg);
  std::abort();
#endif
}
void ThrowStdInvalidArgument(const TString& what_arg) {
#ifdef Y_ABSL_HAVE_EXCEPTIONS
  Throw(std::invalid_argument(what_arg));
#else
  Y_ABSL_RAW_LOG(FATAL, "%s", what_arg.c_str());
  std::abort();
#endif
}
void ThrowStdInvalidArgument(const char* what_arg) {
#ifdef Y_ABSL_HAVE_EXCEPTIONS
  Throw(std::invalid_argument(what_arg));
#else
  Y_ABSL_RAW_LOG(FATAL, "%s", what_arg);
  std::abort();
#endif
}

void ThrowStdDomainError(const TString& what_arg) {
#ifdef Y_ABSL_HAVE_EXCEPTIONS
  Throw(std::domain_error(what_arg));
#else
  Y_ABSL_RAW_LOG(FATAL, "%s", what_arg.c_str());
  std::abort();
#endif
}
void ThrowStdDomainError(const char* what_arg) {
#ifdef Y_ABSL_HAVE_EXCEPTIONS
  Throw(std::domain_error(what_arg));
#else
  Y_ABSL_RAW_LOG(FATAL, "%s", what_arg);
  std::abort();
#endif
}

void ThrowStdLengthError(const TString& what_arg) {
#ifdef Y_ABSL_HAVE_EXCEPTIONS
  Throw(std::length_error(what_arg));
#else
  Y_ABSL_RAW_LOG(FATAL, "%s", what_arg.c_str());
  std::abort();
#endif
}
void ThrowStdLengthError(const char* what_arg) {
#ifdef Y_ABSL_HAVE_EXCEPTIONS
  Throw(std::length_error(what_arg));
#else
  Y_ABSL_RAW_LOG(FATAL, "%s", what_arg);
  std::abort();
#endif
}

void ThrowStdOutOfRange(const TString& what_arg) {
#ifdef Y_ABSL_HAVE_EXCEPTIONS
  Throw(std::out_of_range(what_arg));
#else
  Y_ABSL_RAW_LOG(FATAL, "%s", what_arg.c_str());
  std::abort();
#endif
}
void ThrowStdOutOfRange(const char* what_arg) {
#ifdef Y_ABSL_HAVE_EXCEPTIONS
  Throw(std::out_of_range(what_arg));
#else
  Y_ABSL_RAW_LOG(FATAL, "%s", what_arg);
  std::abort();
#endif
}

void ThrowStdRuntimeError(const TString& what_arg) {
#ifdef Y_ABSL_HAVE_EXCEPTIONS
  Throw(std::runtime_error(what_arg));
#else
  Y_ABSL_RAW_LOG(FATAL, "%s", what_arg.c_str());
  std::abort();
#endif
}
void ThrowStdRuntimeError(const char* what_arg) {
#ifdef Y_ABSL_HAVE_EXCEPTIONS
  Throw(std::runtime_error(what_arg));
#else
  Y_ABSL_RAW_LOG(FATAL, "%s", what_arg);
  std::abort();
#endif
}

void ThrowStdRangeError(const TString& what_arg) {
#ifdef Y_ABSL_HAVE_EXCEPTIONS
  Throw(std::range_error(what_arg));
#else
  Y_ABSL_RAW_LOG(FATAL, "%s", what_arg.c_str());
  std::abort();
#endif
}
void ThrowStdRangeError(const char* what_arg) {
#ifdef Y_ABSL_HAVE_EXCEPTIONS
  Throw(std::range_error(what_arg));
#else
  Y_ABSL_RAW_LOG(FATAL, "%s", what_arg);
  std::abort();
#endif
}

void ThrowStdOverflowError(const TString& what_arg) {
#ifdef Y_ABSL_HAVE_EXCEPTIONS
  Throw(std::overflow_error(what_arg));
#else
  Y_ABSL_RAW_LOG(FATAL, "%s", what_arg.c_str());
  std::abort();
#endif
}
void ThrowStdOverflowError(const char* what_arg) {
#ifdef Y_ABSL_HAVE_EXCEPTIONS
  Throw(std::overflow_error(what_arg));
#else
  Y_ABSL_RAW_LOG(FATAL, "%s", what_arg);
  std::abort();
#endif
}

void ThrowStdUnderflowError(const TString& what_arg) {
#ifdef Y_ABSL_HAVE_EXCEPTIONS
  Throw(std::underflow_error(what_arg));
#else
  Y_ABSL_RAW_LOG(FATAL, "%s", what_arg.c_str());
  std::abort();
#endif
}
void ThrowStdUnderflowError(const char* what_arg) {
#ifdef Y_ABSL_HAVE_EXCEPTIONS
  Throw(std::underflow_error(what_arg));
#else
  Y_ABSL_RAW_LOG(FATAL, "%s", what_arg);
  std::abort();
#endif
}

void ThrowStdBadFunctionCall() {
#ifdef Y_ABSL_HAVE_EXCEPTIONS
  Throw(std::bad_function_call());
#else
  std::abort();
#endif
}

void ThrowStdBadAlloc() {
#ifdef Y_ABSL_HAVE_EXCEPTIONS
  Throw(std::bad_alloc());
#else
  std::abort();
#endif
}

}  // namespace base_internal
Y_ABSL_NAMESPACE_END
}  // namespace y_absl
