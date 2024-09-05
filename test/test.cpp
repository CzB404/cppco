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

#include "fixture.hpp"
#include "libco_mock.hpp"
#include <sstream>
#include <co.hpp>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace cppco_test {

TEST_F(cppco, default_construct)
{
	auto cothread = co::thread();
	EXPECT_FALSE(cothread);
	EXPECT_EQ(cothread.get_stack_size(), co::thread::default_stack_size);
	EXPECT_EQ(&cothread.get_parent(), &co::active());
}

TEST_F(cppco, create_and_destroy)
{
	EXPECT_CALL(libco_mock::api::get(), create(_, _));
	EXPECT_CALL(libco_mock::api::get(), delete_this(_));
	co::thread cothread([]() {});
}

TEST_F(cppco, create_and_reset)
{
	EXPECT_CALL(libco_mock::api::get(), create(_, _));
	EXPECT_CALL(libco_mock::api::get(), switch_to(_)).Times(4); // 2 in test, 2 in reset
	EXPECT_CALL(libco_mock::api::get(), delete_this(_));
	auto cothread = co::thread([]()
	{
		co::active().get_parent().switch_to();
	});
	cothread.switch_to(); // Activate it
	cothread.reset();
}

TEST_F(cppco, move_construct)
{
	bool a = false;
	bool b = false;
	auto cothread = co::thread([&]()
	{
		a = true;
		co::active().get_parent().switch_to();
		b = true;
		co::active().get_parent().switch_to();
	});
	cothread.switch_to();
	EXPECT_TRUE(cothread);
	EXPECT_TRUE(a);
	EXPECT_FALSE(b);
	auto moved_cothread = std::move(cothread);
	EXPECT_FALSE(cothread);
	moved_cothread.switch_to();
	EXPECT_TRUE(a);
	EXPECT_TRUE(b);
}

TEST_F(cppco, move_assign)
{
	bool a = false;
	bool b = false;
	auto cothread = co::thread([&]()
		{
			a = true;
			co::active().get_parent().switch_to();
			b = true;
			co::active().get_parent().switch_to();
		});
	auto moved_cothread = co::thread();
	cothread.switch_to();
	EXPECT_TRUE(cothread);
	EXPECT_TRUE(a);
	EXPECT_FALSE(b);
	moved_cothread = std::move(cothread);
	EXPECT_FALSE(cothread);
	moved_cothread.switch_to();
	EXPECT_TRUE(a);
	EXPECT_TRUE(b);
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
		EXPECT_CALL(libco_mock::api::get(), create(_, _)).Times(1);
		EXPECT_CALL(libco_mock::api::get(), switch_to(_)).Times(4); // 2 in test, 2 in reset
		auto& parent = co::active();
		auto cothread = co::thread([&parent, &destructed]()
		{
			auto a = A(destructed);
			parent.switch_to();
		});
		cothread.switch_to();
		cothread.reset();
	}
	EXPECT_TRUE(destructed);
}

TEST_F(cppco, exception_for_default_cothread)
{
	struct Dummy {};
	auto cothread = co::thread([]()
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
	auto& parent = co::active();
	bool caught = false;
	auto fail_cothread = co::thread([&parent, &caught]()
	{
		try
		{
			parent.switch_to();
		}
		catch (const Dummy&)
		{
			caught = true;
			parent.switch_to();
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
	auto& parent = co::active();
	bool caught = false;
	auto cothread = co::thread([]()
		{
			throw Dummy();
		});
	auto fail_cothread = co::thread([&parent, &caught]()
		{
			try
			{
				parent.switch_to();
			}
			catch (const Dummy&)
			{
				caught = true;
				parent.switch_to();
			}
		});
	cothread.set_parent(fail_cothread);
	fail_cothread.switch_to();
	cothread.switch_to();
	EXPECT_TRUE(caught);
}

TEST_F(cppco, reset_entry)
{
	auto& parent = co::active();
	auto cothread = co::thread([]() {});
	bool run = false;
	cothread.reset([&]()
	{
		run = true;
		parent.switch_to();
	});
	cothread.switch_to();
	EXPECT_TRUE(run);
}

TEST_F(cppco, rewind)
{
	bool a = false;
	bool b = false;
	auto& parent = co::active();
	auto cothread = co::thread([&]()
	{
		a = true;
		parent.switch_to();
		b = true;
		parent.switch_to();
	});
	cothread.switch_to();
	EXPECT_TRUE(a);
	EXPECT_FALSE(b);
	a = false;
	b = false;
	cothread.rewind();
	cothread.switch_to();
	EXPECT_TRUE(a);
	EXPECT_FALSE(b);
}

TEST_F(cppco, rewind_after_exception)
{
	class Dummy {};
	auto cothread = co::thread([]()
	{
		co::active().get_parent().switch_to();
		throw Dummy{};
	});
	cothread.switch_to(); // Activate it
	EXPECT_THROW(cothread.switch_to(), Dummy);
	EXPECT_FALSE(cothread);
	cothread.rewind();
	cothread.switch_to(); // Activate it
	EXPECT_TRUE(cothread);
	EXPECT_THROW(cothread.switch_to(), Dummy);
}

TEST_F(cppco, set_stack_size)
{
	EXPECT_CALL(::libco_mock::api::get(), create(_, _)).Times(0);
	auto cothread = co::thread();
	EXPECT_FALSE(cothread);
	EXPECT_EQ(cothread.get_stack_size(), co::thread::default_stack_size);
	EXPECT_CALL(::libco_mock::api::get(), create(_, _)).Times(0);
	cothread.set_stack_size(2 * co::thread::default_stack_size);
	EXPECT_EQ(cothread.get_stack_size(), 2 * co::thread::default_stack_size);
}

TEST_F(cppco, set_stack_size_when_active)
{
	EXPECT_CALL(::libco_mock::api::get(), create(co::thread::default_stack_size, _)).Times(1);
	auto cothread = co::thread([]()
	{
		co::active().get_parent().switch_to();
	});
	cothread.switch_to(); // Activate it
	EXPECT_TRUE(cothread);
	EXPECT_EQ(cothread.get_stack_size(), co::thread::default_stack_size);
	EXPECT_CALL(::libco_mock::api::get(), create(2 * co::thread::default_stack_size, _)).Times(1);
	cothread.set_stack_size(2 * co::thread::default_stack_size);
	EXPECT_EQ(cothread.get_stack_size(), 2 * co::thread::default_stack_size);
}

TEST_F(cppco, set_active_as_main)
{
	struct cothread_deleter
	{
		void operator()(cothread_t p) const noexcept
		{
			co_delete(p);
		}
	};

	static bool a = false;
	static bool b = false;
	static bool c = false;

	static cothread_t parent = co_active();

	static auto* pcothread = static_cast<co::thread*>(nullptr);

	auto ccothread = std::unique_ptr<void, cothread_deleter>(co_create(static_cast<int>(co::thread::default_stack_size), +[]()
	{
		a = true;
		co::set_active_as_main();
		pcothread->set_parent(co::active());
		pcothread->switch_to();
		c = true;
		co_switch(parent);
	}));

	auto cothread = co::thread([]()
	{
		b = true;
		co::active().get_parent().switch_to();
	});

	pcothread = &cothread;

	co_switch(ccothread.get());
	EXPECT_TRUE(a);
	EXPECT_TRUE(b);
	EXPECT_TRUE(c);

	// Ensure safe destruction
	co::set_active_as_main();
}

} // namespace cppco_test
