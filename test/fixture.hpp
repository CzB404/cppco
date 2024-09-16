// Copyright(C) 2024 by Balazs Cziraki <balazs.cziraki@gmail.com>
//
// Permission to use, copy, modify, and /or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above copyright
// notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
// OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
// TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
// THIS SOFTWARE.

#ifndef CPPCO_TEST_FIXTURE_HPP_INCLUDE_GUARD
#define CPPCO_TEST_FIXTURE_HPP_INCLUDE_GUARD

#include <map>
#include <thread>
#include <memory>
#include "libco_mock.hpp"
#include <co.hpp>

namespace cppco_test {

using namespace ::testing;

class cppco : public ::testing::Test
{
	using Base = ::testing::Test;
public:
	static std::mutex s_status_storage_mutex;
	using status_storage = std::map<std::thread::id, std::shared_ptr<void>>;
	static status_storage s_status_storage;

	void reset_status();

	virtual void SetUp() override;
	virtual void TearDown() override;
};

} // namespace cppco_test

#endif // CPPCO_TEST_FIXTURE_HPP_INCLUDE_GUARD
