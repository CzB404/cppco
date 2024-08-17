#include <co.hpp>
#include <cstdio>

template<typename T>
class tracer
{
public:
	tracer() noexcept
	{
		std::printf("%s::%s()\n", T::s_type_name, T::s_type_name);
	}
	tracer(const tracer&) noexcept
	{
		std::printf("%s::%s(const %s&)\n", T::s_type_name, T::s_type_name, T::s_type_name);
	}
	tracer(tracer&&) noexcept
	{
		std::printf("%s::%s(const %s&)\n", T::s_type_name, T::s_type_name, T::s_type_name);
	}
	tracer& operator=(const tracer&) noexcept
	{
		std::printf("%s::operator=(const %s&)\n", T::s_type_name, T::s_type_name);
		return *this;
	}
	tracer& operator=(tracer&&) noexcept
	{
		std::printf("%s::operator=(const %s&)\n", T::s_type_name, T::s_type_name);
		return *this;
	}
	~tracer() noexcept
	{
		std::printf("%s::~%s()\n", T::s_type_name, T::s_type_name);
	}
};

class A : private tracer<A>
{
	friend tracer<A>;
	inline static constexpr const char* s_type_name = "A";
};

int main()
{
	auto current = co::thread::current();
	auto cothread = co::thread([current]()
	{
		A a;
		current.switch_to();
		std::printf("%s\n", "Hello Wolrd!");
		current.switch_to();
	});
	cothread.switch_to();
	return 0;
}
