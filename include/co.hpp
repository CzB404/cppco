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

#ifndef CO_HPP_INCLUDE_GUARD
#define CO_HPP_INCLUDE_GUARD

#include <libco.h>
#include <memory>
#include <functional>

namespace co {

class thread;

class thread_failure;
class thread_create_failure;
class thread_return_failure;

const thread& active() noexcept;

class thread_failure : public std::runtime_error
{
public:
	using std::runtime_error::runtime_error;
};

class thread_create_failure : public thread_failure
{
public:
	thread_create_failure() noexcept;
};
class thread_return_failure : public thread_failure
{
public:
	thread_return_failure() noexcept;
};

class thread
{
public:
	using entry_t = std::function<void()>;

	// libco's recommendation for the default stack size is 1 MB on 32 bit systems, and to define the stack size in pointer size.
	inline static constexpr size_t default_stack_size = 1 * 1024 * 1024 / 4 * sizeof(void*);

	explicit operator bool() const noexcept;
	void switch_to() const;
	void reset();
	void reset(entry_t entry);
	void rewind();

	friend const thread& active() noexcept;

	size_t get_stack_size() const noexcept;
	void set_stack_size(size_t stack_size) noexcept;

	void set_parent(const thread& parent) noexcept;
	const thread& get_parent() const noexcept;

	explicit thread(size_t stack_size = default_stack_size);
	explicit thread(const thread& parent, size_t stack_size = default_stack_size);
	explicit thread(entry_t entry, const thread& parent = active());
	explicit thread(entry_t entry, size_t stack_size, const thread& parent = active());

	thread(const thread& other) = delete;
	thread(thread&& other) = delete;
	thread& operator=(const thread& other) = delete;
	thread& operator=(thread&& other) = delete;
	~thread();

private:
	class private_token_t {};
	inline static constexpr private_token_t private_token;
public:
	explicit thread(cothread_t cothread, private_token_t) noexcept;

private:
	class thread_stopping : public std::exception
	{
	};

	struct thread_deleter
	{
		void operator()(cothread_t p) const noexcept;
	};

	struct thread_status
	{
		std::unique_ptr<thread> main;
		const thread* current_active = nullptr;
		const thread* current_thread = nullptr;
		std::exception_ptr current_exception;

		thread_status() noexcept;
	};

	void setup();
	static void entry_wrapper() noexcept;

	using thread_ptr = std::unique_ptr<void, thread_deleter>;

	cothread_t get_thread() const noexcept;
	void stop() const noexcept;

	static thread_local std::unique_ptr<thread_status> tl_status;

	thread_ptr m_thread;
	const thread* m_parent = nullptr;
	entry_t m_entry;
	size_t m_stack_size = 0;
	mutable bool m_active = false;
};

} // namespace co

#include "co.ipp"

#endif // CO_HPP_INCLUDE_GUARD
