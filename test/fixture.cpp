#include "fixture.hpp"

namespace cppco_test {

std::mutex cppco::s_status_storage_mutex;
cppco::status_storage cppco::s_status_storage;

void cppco::reset_status()
{
	std::lock_guard<std::mutex> guard(s_status_storage_mutex);
	s_status_storage.clear();
}

void cppco::SetUp()
{
	Base::SetUp();
	reset_status();
}

void cppco::TearDown()
{
	libco_mock::api::verify();
	reset_status();
	Base::TearDown();
}

} // namespace cppco_test

co::thread::thread_status& co::thread::status()
{
	std::lock_guard<std::mutex> guard(cppco_test::cppco::s_status_storage_mutex);
	auto&& storage = cppco_test::cppco::s_status_storage;
	auto id = std::this_thread::get_id();
	auto it = storage.find(id);
	if (it != storage.end())
	{
		return *std::static_pointer_cast<co::thread::thread_status>(it->second);
	}
	auto result = storage.insert(std::make_pair(id, std::static_pointer_cast<void>(std::make_shared<co::thread::thread_status>())));
	assert(result.second);
	return *std::static_pointer_cast<co::thread::thread_status>(result.first->second);
}
