#ifndef CO_IPP_INCLUDE_GUARD
#define CO_IPP_INCLUDE_GUARD

#include <stdexcept>
#include <type_traits>

namespace co {

template<typename T>
inline cothread_t detail::thread_impl<T>::get_thread() const noexcept
{
	static_assert(std::is_base_of_v<thread_impl<T>, T>, "co::detail::thread_impl<T> is a CRTP class, T should inherit from co::detail::thread_impl<T>!");
	return static_cast<const T*>(this)->get_thread();
}

inline cothread_t thread_ref::get_thread() const noexcept
{
	return m_thread;
}

template<typename Entry>
inline cothread_t thread<Entry>::get_thread() const noexcept
{
	return m_thread.get();
}

template<typename T>
inline detail::thread_impl<T>::operator bool() const noexcept
{
	return get_thread() != nullptr;
}

template<typename T>
inline void detail::thread_impl<T>::switch_to() const
{
	co_switch(get_thread());
	if (tl_current_thread)
	{
		throw thread_deletion();
	}
}

inline thread_ref current_thread() noexcept
{
	return thread_ref(co_active());
}

inline thread_ref::thread_ref(cothread_t thread) noexcept
	: m_thread{ thread }
{
}

inline void detail::thread_base::thread_deleter::operator()(cothread_t p) const noexcept
{
	co_delete(p);
}

template<typename Entry>
inline thread<Entry>::thread() = default;
template<typename Entry>
inline thread<Entry>::~thread()
{
	tl_current_thread = co_active();
	switch_to();
}

template<typename Entry>
inline thread<Entry>::thread(thread<Entry>::entry_t entry, size_t stack_size)
	: m_entry{ std::move(entry) }
{
	tl_current_this = this;
	tl_current_thread = co_active();
	m_thread.reset(co_create(static_cast<unsigned int>(stack_size), &entry_wrapper));
	co_switch(m_thread.get());
}

inline thread_local detail::thread_base* detail::thread_base::tl_current_this = nullptr;
inline thread_local cothread_t detail::thread_base::tl_current_thread = nullptr;

template<typename Entry>
inline void thread<Entry>::entry_wrapper() noexcept
{
	auto* self = static_cast<thread<Entry>*>(std::exchange(tl_current_this, nullptr));
	auto* creating_thread = std::exchange(tl_current_thread, nullptr);
	co_switch(creating_thread);
	try
	{
		(self->m_entry)();
	}
	catch (const thread_deletion&)
	{
		auto* deleting_thread = std::exchange(tl_current_thread, nullptr);
		co_switch(deleting_thread);
	}
	catch(...)
	{
		self->m_exception = std::current_exception();
		co_switch(creating_thread);
	}
}

} // namespace co

#endif // CO_IPP_INCLUDE_GUARD
