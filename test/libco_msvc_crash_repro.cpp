#include <stdexcept>
#include <cstdio>
#include <libco.h>

cothread_t parent;

void will_throw()
{
	throw std::runtime_error("Catch me!");
}

void entry()
{
	try
	{
		will_throw();
	}
	catch (const std::exception& e)
	{
		std::printf("%s\n", e.what());
		co_switch(parent);
	}
}

int main()
{
	parent = co_active();
	auto cothread = co_create(262144 * sizeof(void*), &entry);
	co_switch(cothread);
	return 0;
}
