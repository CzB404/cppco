// Copyright(C) 2024 by Balazs Cziraki <balazs.cziraki@gmail.com>
//
// Permission to use, copy, modify, and /or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above copyright
// notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS.IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
// OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
// TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
// THIS SOFTWARE.

#include "libco_mock.hpp"
#include <co.hpp>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace cppco_test {

using namespace ::testing;

class cppco : public ::testing::Test
{
	using Base = ::testing::Test;
public:
	virtual void SetUp() override
	{
		Base::SetUp();
	}
	virtual void TearDown() override
	{
		libco_mock::api::verify();
		Base::TearDown();
	}
};

TEST_F(cppco, create_and_destroy)
{
	EXPECT_CALL(libco_mock::api::get(), active()).Times(3); // Default failure cothread is the current cothread. + Constructor needs to initialize entry function and has to call back home. + Destructor kills the entry scope and also has to call back home.
	EXPECT_CALL(libco_mock::api::get(), create(_, _));
	EXPECT_CALL(libco_mock::api::get(), switch_to(_)).Times(4); // 2 in constructor, 2 in destructor
	EXPECT_CALL(libco_mock::api::get(), delete_this(_));
	auto cothread = co::thread([]() {});
}

TEST_F(cppco, create_and_reset)
{
	EXPECT_CALL(libco_mock::api::get(), active()).Times(3);
	EXPECT_CALL(libco_mock::api::get(), create(_, _));
	EXPECT_CALL(libco_mock::api::get(), switch_to(_)).Times(4); // 2 in constructor, 2 in reset
	EXPECT_CALL(libco_mock::api::get(), delete_this(_));
	auto cothread = co::thread([]() {});
	cothread.reset();
}

TEST_F(cppco, creation_failure)
{
	EXPECT_CALL(libco_mock::api::get(), create(_, _)).WillOnce(Return(nullptr));
	EXPECT_THROW(co::thread([]() {}), co::thread_create_failure);
}

TEST_F(cppco, destructors)
{
	bool destructed = false;

	class A
	{
	public:
		A(bool& destructed)
			: m_destructed{ &destructed }
		{
		}
		~A()
		{
			*m_destructed = true;
		}
	private:
		bool* m_destructed;
	};

	{
		EXPECT_CALL(libco_mock::api::get(), active()).Times(4); // current_thread + constructor + default failure_thread + destructor
		EXPECT_CALL(libco_mock::api::get(), create(_, _)).Times(1);
		EXPECT_CALL(libco_mock::api::get(), switch_to(_)).Times(6); // 2 in main, 2 in constructor, 2 in reset
		auto current = co::current_thread();
		auto cothread = co::thread([current, &destructed]()
		{
			auto a = A(destructed);
			current.switch_to();
		});
		cothread.switch_to();
		cothread.reset();
	}
	EXPECT_TRUE(destructed);
}

TEST_F(cppco, exception_for_default_cothread)
{
	struct Dummy {};
	auto current = co::current_thread();
	auto cothread = co::thread([current]()
	{
		throw Dummy();
	});
	EXPECT_THROW(cothread.switch_to(), Dummy);
}

TEST_F(cppco, returning_entry)
{
	auto cothread = co::thread([]()	{});
	EXPECT_THROW(cothread.switch_to(), co::thread_return_failure);
}

TEST_F(cppco, exception_for_other_cothread)
{
	struct Dummy {};
	auto current = co::current_thread();
	bool caught = false;
	auto fail_cothread = co::thread([current, &caught]()
	{
		try
		{
			current.switch_to();
		}
		catch (const Dummy&)
		{
			caught = true;
			current.switch_to();
		}
	});
	fail_cothread.switch_to();
	auto cothread = co::thread([]()
	{
		throw Dummy();
	},
		fail_cothread);
	cothread.switch_to();
	EXPECT_TRUE(caught);
}

TEST_F(cppco, exception_for_new_cothread)
{
	struct Dummy {};
	auto current = co::current_thread();
	bool caught = false;
	auto cothread = co::thread([]()
		{
			throw Dummy();
		});
	auto fail_cothread = co::thread([current, &caught]()
		{
			try
			{
				current.switch_to();
			}
			catch (const Dummy&)
			{
				caught = true;
				current.switch_to();
			}
		});
	cothread.set_failure_thread(fail_cothread);
	fail_cothread.switch_to();
	cothread.switch_to();
	EXPECT_TRUE(caught);
}

TEST_F(cppco, reset_entry)
{
	auto current = co::current_thread();
	auto cothread = co::thread([]() {});
	bool run = false;
	cothread.reset([&]()
	{
		run = true;
		current.switch_to();
	});
	cothread.switch_to();
	EXPECT_TRUE(run);
}

} // namespace cppco_test
