#include "filestream.h"

OptionalOwnerBuffer::OptionalOwnerBuffer() noexcept
{
	data.p = nullptr;
	ownership = false;
}

OptionalOwnerBuffer::OptionalOwnerBuffer(OptionalOwnerBuffer&& o) noexcept
{
	ownership = o.ownership;
	if (ownership)
	{
		data.v = std::move(o.data.v);
	}
	else
	{
		data.p = o.data.p;
	}
}

OptionalOwnerBuffer::OptionalOwnerBuffer(std::vector<std::byte>&& in) noexcept
{
	set(std::move(in));
}

OptionalOwnerBuffer::OptionalOwnerBuffer(const std::byte* in) noexcept
{
	set(in);
}

void OptionalOwnerBuffer::set(std::vector<std::byte>&& in) noexcept
{
	data.v = std::move(in);
	ownership = true;
}

void OptionalOwnerBuffer::set(const std::byte* in) noexcept
{
	data.p = in;
	ownership = false;
}

const std::byte* OptionalOwnerBuffer::get()  const noexcept
{
	return ownership ? data.v.data() : data.p;
}

