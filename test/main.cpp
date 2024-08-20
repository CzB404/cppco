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
	EXPECT_CALL(libco_mock::api::get(), create(_, _));
	EXPECT_CALL(libco_mock::api::get(), switch_to(_)).Times(4); // 2 in constructor, 2 in destructor
	EXPECT_CALL(libco_mock::api::get(), delete_this(_));
	auto cothread = co::thread([]() {});
}

TEST_F(cppco, create_and_reset)
{
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
		EXPECT_CALL(libco_mock::api::get(), active()).Times(3); // current_thread + constructor + destructor
		EXPECT_CALL(libco_mock::api::get(), create(_, _)).Times(1);
		EXPECT_CALL(libco_mock::api::get(), switch_to(_)).Times(5); // 2 in main, 2 in constructor, 2 in reset...?
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

} // namespace cppco_test
