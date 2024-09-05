#include "fixture.hpp"

namespace cppco_test {

std::mutex cppco::s_status_storage_mutex;
cppco::status_storage cppco::s_status_storage;

} // namespace cppco_test
