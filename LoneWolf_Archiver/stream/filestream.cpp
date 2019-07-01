#include <cassert>

#include "filestream.h"

namespace stream
{
	OptionalOwnerBuffer::OptionalOwnerBuffer() noexcept
	{
		data.pc = nullptr;
		mode = pc;
	}

	OptionalOwnerBuffer::OptionalOwnerBuffer(OptionalOwnerBuffer&& o) noexcept
	{
		switch (o.mode)
		{
		case v:
			new(&data.v) std::vector<std::byte>(std::move(o.data.v));
			break;
		case p:
			data.p = o.data.p;
			break;
		default: // pc
			data.pc = o.data.pc;
			break;
		}
		mode = o.mode;
	}

	OptionalOwnerBuffer& OptionalOwnerBuffer::operator=(OptionalOwnerBuffer&& o) noexcept
	{
		switch (o.mode)
		{
		case v:
			if (v == mode) data.v = std::move(o.data.v);
			else new(&data.v) std::vector<std::byte>(std::move(o.data.v));
			break;
		case p:
			if (v == mode) data.v.~vector();
			data.p = o.data.p;
			break;
		default: // pc
			if (v == mode) data.v.~vector();
			data.pc = o.data.pc;
			break;
		}
		mode = o.mode;
		return *this;
	}

	OptionalOwnerBuffer::OptionalOwnerBuffer(std::vector<std::byte>&& in) noexcept
	{
		new(&data.v) std::vector<std::byte>(std::move(in));
		mode = v;
	}

	OptionalOwnerBuffer&
		OptionalOwnerBuffer::operator=(std::vector<std::byte>&& in) noexcept
	{
		if (v == mode) data.v = std::move(in);
		else new(&data.v) std::vector<std::byte>(std::move(in));

		mode = v;
		return *this;
	}

	OptionalOwnerBuffer::OptionalOwnerBuffer(std::byte* in) noexcept
	{
		data.p = in;
		mode = p;
	}

	OptionalOwnerBuffer& OptionalOwnerBuffer::operator=(std::byte* in) noexcept
	{
		if (v == mode) data.v.~vector();

		data.p = in;
		mode = p;
		return *this;
	}

	OptionalOwnerBuffer::OptionalOwnerBuffer(const std::byte* in) noexcept
	{
		data.pc = in;
		mode = pc;
	}

	OptionalOwnerBuffer& OptionalOwnerBuffer::operator=(const std::byte* in) noexcept
	{
		if (v == mode) data.v.~vector();

		data.pc = in;
		mode = pc;
		return *this;
	}

	OptionalOwnerBuffer::~OptionalOwnerBuffer()
	{
		if (v == mode) data.v.~vector();
	}

	std::byte* OptionalOwnerBuffer::get() noexcept
	{
		switch (mode)
		{
		case v:
			return data.v.data();
		case p:
			return data.p;
		default: // pc
			assert(false);
			return nullptr;
		}
	}

	const std::byte* OptionalOwnerBuffer::get_const() const noexcept
	{
		switch (mode)
		{
		case v:
			return data.v.data();
		case p:
			return data.p;
		default: // pc
			return data.pc;
		}
	}
}
