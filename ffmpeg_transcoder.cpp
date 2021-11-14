#include "ffmpeg_transcoder.h"

FFmpegTranscoder::FFmpegTranscoder()
{
	m_sws_context = NULL;
	m_sws_frame_buffer = NULL;
	m_sws_frame = NULL;
	m_src_width = -1;
	m_src_height = -1;
	m_src_pixel_format = AVPixelFormat::AV_PIX_FMT_NONE;
}

FFmpegTranscoder::~FFmpegTranscoder()
{
	if (m_sws_frame)
	{
		av_frame_free(&m_sws_frame);
		m_sws_frame = NULL;
	}

	if (m_sws_frame_buffer)
	{
		av_free(m_sws_frame_buffer);
		m_sws_frame_buffer = NULL;
	}

	if (m_sws_context)
	{
		sws_freeContext(m_sws_context);
		m_sws_context = NULL;
	}
}

void FFmpegTranscoder::free_context()
{
	if (m_sws_frame)
	{
		av_frame_free(&m_sws_frame);
		m_sws_frame = NULL;
	}

	if (m_sws_frame_buffer)
	{
		av_free(m_sws_frame_buffer);
		m_sws_frame_buffer = NULL;
	}

	if (m_sws_context)
	{
		sws_freeContext(m_sws_context);
		m_sws_context = NULL;
	}
}

bool FFmpegTranscoder::scale_yuv(uint8_t *data, int* linesize, int width, int height, AVPixelFormat format, AVFrame** frame)
{
	int ret;
	if (width != m_src_width || height != m_src_height || format != m_src_pixel_format)
	{
		free_context();
	}
	m_sws_context = sws_getCachedContext(m_sws_context, width, height, format, width, height,
		AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, NULL, NULL, NULL);
	if (m_sws_context)
	{
		if (!m_sws_frame)
		{
			m_sws_frame = av_frame_alloc();
			if (!m_sws_frame)
			{
				return false;
			}
			ret = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, width, height, 1);
			if (ret < 0)
			{
				return false;
			}
			m_sws_frame_buffer = (uint8_t *)av_malloc(ret);
			if (!m_sws_frame_buffer)
			{
				return false;
			}
			ret = av_image_fill_arrays(m_sws_frame->data, m_sws_frame->linesize, m_sws_frame_buffer,
				AV_PIX_FMT_YUV420P, width, height, 1);
			if (ret < 0)
			{
				return false;
			}
		}

		sws_scale(m_sws_context, (const uint8_t * const *)data, linesize,
			0, height, m_sws_frame->data, m_sws_frame->linesize);

		*frame = m_sws_frame;
		return true;
	}
	else
	{
		return false;
	}
}