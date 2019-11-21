#include <cassert>

#include "filestream.h"

namespace stream
{
	std::byte* OptionalOwnerBuffer::get()
	{
		if (std::holds_alternative<std::byte*>(data))
		{
			return std::get<std::byte*>(data);
		}
		if (std::holds_alternative<std::vector<std::byte>>(data))
		{
			return std::get<std::vector<std::byte>>(data).data();
		}
		assert(false);
		return nullptr;
	}

	const std::byte* OptionalOwnerBuffer::get_const() const
	{
		if (std::holds_alternative<const std::byte*>(data))
		{
			return std::get<const std::byte*>(data);
		}
		if (std::holds_alternative<std::byte*>(data))
		{
			return std::get<std::byte*>(data);
		}
		if (std::holds_alternative<std::vector<std::byte>>(data))
		{
			return std::get<std::vector<std::byte>>(data).data();
		}
		assert(false);
		return nullptr;
	}
	void OptionalOwnerBuffer::reset()
	{
		data = decltype(data)();
	}
}
