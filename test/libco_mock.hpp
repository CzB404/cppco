#ifndef LIBCO_MOCK_HPP_INCLUDE_GUARD
#define LIBCO_MOCK_HPP_INCLUDE_GUARD

#include <libco.h>
#include <gmock/gmock.h>

namespace libco_mock {

class api
{
public:
	MOCK_METHOD(cothread_t, active, (), (const, noexcept));
	MOCK_METHOD(cothread_t, derive, (void*, unsigned int, void (*)()), (const, noexcept));
	MOCK_METHOD(cothread_t, create, (unsigned int, void (*)()), (const, noexcept));
	MOCK_METHOD(void, delete_this, (cothread_t), (const, noexcept));
	MOCK_METHOD(void, switch_to, (cothread_t), (const, noexcept));
	MOCK_METHOD(int, serializable, (), (const, noexcept));

	static api& get()
	{
		using namespace ::testing;
		static NiceMock<api> instance;
		return instance;
	}

	static void verify()
	{
		using namespace ::testing;
		Mock::VerifyAndClearExpectations(&get());
	}
protected:
	api()
	{
		using namespace ::testing;
		ON_CALL(*this, active()).WillByDefault(Invoke(co_active));
		ON_CALL(*this, derive(_, _, _)).WillByDefault(Invoke(co_derive));
		ON_CALL(*this, create(_, _)).WillByDefault(Invoke(co_create));
		ON_CALL(*this, delete_this(_)).WillByDefault(Invoke(co_delete));
		ON_CALL(*this, switch_to(_)).WillByDefault(Invoke(co_switch));
		ON_CALL(*this, serializable()).WillByDefault(Invoke(co_serializable));

	}
	~api() = default;
};

inline cothread_t active() noexcept
{
	return api::get().active();
}
inline cothread_t derive(void* p , unsigned int s, void (*x)()) noexcept
{
	return api::get().derive(p, s, x);
}
inline cothread_t create(unsigned int s, void (*x)()) noexcept
{
	return api::get().create(s, x);
}
inline void delete_this(cothread_t p) noexcept
{
	return api::get().delete_this(p);
}
inline void switch_to(cothread_t p) noexcept
{
	return api::get().switch_to(p);
}
inline int serializable() noexcept
{
	return api::get().serializable();
}

} // namespace libco_mock

#define co_active ::libco_mock::active
#define co_derive ::libco_mock::derive
#define co_create ::libco_mock::create
#define co_delete ::libco_mock::delete_this
#define co_switch ::libco_mock::switch_to
#define co_serializable ::libco_mock::serializable

#endif // LIBCO_MOCK_HPP_INCLUDE_GUARD
