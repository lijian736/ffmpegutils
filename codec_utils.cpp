#include "codec_utils.h"

static const uint8_t *AVCFindStartCodeInternal(const uint8_t *start, const uint8_t *end)
{
	const uint8_t *a = start + 4 - ((intptr_t)start & 3);

	for (end -= 3; start < a && start < end; start++)
	{
		if (start[0] == 0 && start[1] == 0 && start[2] == 1)
		{
			return start;
		}
	}

	for (end -= 3; start < end; start += 4)
	{
		uint32_t x = *(const uint32_t *)start;

		if ((x - 0x01010101) & (~x) & 0x80808080)
		{
			if (start[1] == 0)
			{
				if (start[0] == 0 && start[2] == 1)
				{
					return start;
				}
				if (start[2] == 0 && start[3] == 1)
				{
					return start + 1;
				}
			}

			if (start[3] == 0)
			{
				if (start[2] == 0 && start[4] == 1)
				{
					return start + 2;
				}
				if (start[4] == 0 && start[5] == 1)
				{
					return start + 3;
				}
			}
		}
	}

	for (end += 3; start < end; start++)
	{
		if (start[0] == 0 && start[1] == 0 && start[2] == 1)
		{
			return start;
		}
	}

	return end + 3;
}

const uint8_t* avc_find_start_code(const uint8_t *start, const uint8_t *end)
{
	const uint8_t *pos = AVCFindStartCodeInternal(start, end);
	if (start < pos && pos < end && !pos[-1])
	{
		pos--;
	}

	return pos;
}

bool avc_find_key_frame(const uint8_t *data, size_t size)
{
	const uint8_t *nalStart;
	const uint8_t *nalEnd;
	const uint8_t *end = data + size;
	int type;

	nalStart = avc_find_start_code(data, end);
	while (true)
	{
		while (nalStart < end && !*(nalStart++))
			;

		if (nalStart == end)
		{
			break;
		}

		type = nalStart[0] & 0x1F;

		if (type == 5 || type == 1)
		{
			return (type == 5);
		}

		nalEnd = avc_find_start_code(nalStart, end);
		nalStart = nalEnd;
	}

	return false;
}

int count_avc_key_frames(const uint8_t *data, size_t size)
{
	int count = 0;
	const uint8_t *nalStart;
	const uint8_t *nalEnd;
	const uint8_t *end = data + size;
	int type;

	nalStart = avc_find_start_code(data, end);
	while (true)
	{
		while (nalStart < end && !*(nalStart++))
			;

		if (nalStart == end)
		{
			break;
		}

		type = nalStart[0] & 0x1F;

		if (type == 5)
		{
			count++;
		}

		nalEnd = avc_find_start_code(nalStart, end);
		nalStart = nalEnd;
	}

	return count;
}

int count_frames(const uint8_t *data, size_t size)
{
	int count = 0;
	const uint8_t *nalStart;
	const uint8_t *nalEnd;
	const uint8_t *end = data + size;

	nalStart = avc_find_start_code(data, end);
	while (true)
	{
		while (nalStart < end && !*(nalStart++))
			;

		if (nalStart == end)
		{
			break;
		}

		count++;
		nalEnd = avc_find_start_code(nalStart, end);
		nalStart = nalEnd;
	}

	return count;
}