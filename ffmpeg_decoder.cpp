#include "ffmpeg_decoder.h"
#include "codec_utils.h"

namespace
{
	static enum AVPixelFormat hw_pix_fmt;

	enum AVHWDeviceType hw_priorities[] = {
		AV_HWDEVICE_TYPE_D3D11VA,
		AV_HWDEVICE_TYPE_CUDA,
		AV_HWDEVICE_TYPE_VDPAU,
		AV_HWDEVICE_TYPE_VAAPI,
		AV_HWDEVICE_TYPE_DXVA2,
		AV_HWDEVICE_TYPE_QSV,
		AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
		AV_HWDEVICE_TYPE_DRM,
		AV_HWDEVICE_TYPE_OPENCL,
		AV_HWDEVICE_TYPE_MEDIACODEC,
		AV_HWDEVICE_TYPE_VULKAN,
		AV_HWDEVICE_TYPE_NONE
	};

	enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts)
	{
		const enum AVPixelFormat *p;

		for (p = pix_fmts; *p != -1; p++) 
		{
			if (*p == hw_pix_fmt)
			{
				return *p;
			}
		}

		return AV_PIX_FMT_NONE;
	}
}

FFmpegDecoder::FFmpegDecoder()
{
	m_decoder_context = NULL;
	m_decoder_codec = NULL;
	m_hw_frame = NULL;
	m_frame = NULL;
	m_sws_frame = NULL;
	m_sws_context = NULL;
	m_sws_frame_buffer = NULL;
	m_hw_ctx = NULL;

	m_hw_available = false;
	m_initialized = false;
}

FFmpegDecoder::~FFmpegDecoder()
{
	free_context();
}

bool FFmpegDecoder::init(enum AVCodecID id)
{
	int ret;

	free_context();

	m_decoder_codec = avcodec_find_decoder(id);
	if (!m_decoder_codec)
	{
		return false;
	}

	m_decoder_context = avcodec_alloc_context3(m_decoder_codec);
	if (!m_decoder_context)
	{
		return false;
	}

	m_decoder_context->thread_count = 4;
	m_decoder_context->thread_type = FF_THREAD_SLICE;
	
	m_decoder_context->hw_device_ctx = NULL;
	//initialize the hardware decoder
	m_hw_available = init_hw_decoder();
	if (m_hw_available)
	{
		m_decoder_context->get_format = get_hw_format;
	}

	ret = avcodec_open2(m_decoder_context, m_decoder_codec, NULL);
	if (ret < 0)
	{
		free_context();
		return false;
	}

#if LIBAVCODEC_VERSION_MAJOR >= 58
	if (m_decoder_codec->capabilities & AV_CODEC_CAP_TRUNCATED)
	{
		//we don't send complete frames
		m_decoder_context->flags |= AV_CODEC_FLAG_TRUNCATED;
	}
#else
	if (m_decoder_codec->codec->capabilities & CODEC_CAP_TRUNCATED)
	{
		m_decoder_context->decoder->flags |= CODEC_FLAG_TRUNCATED;
	}
#endif

	m_initialized = true;
	return true;
}

bool FFmpegDecoder::init_hw_decoder()
{
	enum AVHWDeviceType *priority = hw_priorities;
	AVBufferRef *hwCtx = NULL;

	while (*priority != AV_HWDEVICE_TYPE_NONE)
	{
		if (has_hw_type(*priority))
		{
			int ret = av_hwdevice_ctx_create(&hwCtx, *priority, NULL, NULL, 0);
			if (ret == 0)
			{
				break;
			}
		}
		priority++;
	}

	if (hwCtx)
	{
		m_hw_ctx = hwCtx;
		m_decoder_context->hw_device_ctx = av_buffer_ref(hwCtx);
		return true;
	}

	return false;
}

bool FFmpegDecoder::has_hw_type(enum AVHWDeviceType type)
{
	for (int i = 0;; i++)
	{
		const AVCodecHWConfig *config = avcodec_get_hw_config(m_decoder_codec, i);
		if (!config) 
		{
			break;
		}

		if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
			config->device_type == type)
		{
			hw_pix_fmt = config->pix_fmt;
			return true;
		}
	}

	return false;
}

bool FFmpegDecoder::validate(enum AVCodecID id)
{
	if (!m_initialized)
	{
		return false;
	}

	if (m_decoder_codec->id != id)
	{
		return false;
	}

	return true;
}

bool FFmpegDecoder::free_context()
{
	if (m_decoder_context)
	{
		avcodec_free_context(&m_decoder_context);
		m_decoder_context = NULL;
	}

	if (m_hw_ctx)
	{
		av_buffer_unref(&m_hw_ctx);
		m_hw_ctx = NULL;
	}

	if (m_hw_frame)
	{
		av_frame_free(&m_hw_frame);
		m_hw_frame = NULL;
	}
	
	if (m_frame)
	{
		av_frame_free(&m_frame);
		m_frame = NULL;
	}

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

	m_initialized = false;

	return true;
}

bool FFmpegDecoder::send_video_data(uint8_t* data, size_t size, long long timestamp)
{
	AVPacket packet = { 0 };
	int ret;

#if LIBAVCODEC_VERSION_MAJOR >= 58
	size_t padding_size = size + AV_INPUT_BUFFER_PADDING_SIZE;
#else
	size_t padding_size = size + FF_INPUT_BUFFER_PADDING_SIZE;
#endif

	packet.data = data;
	packet.size = (int)size;
	packet.pts = timestamp;

	if (m_decoder_codec->id == AV_CODEC_ID_H264 && avc_find_key_frame(data, size))
	{
		packet.flags |= AV_PKT_FLAG_KEY;
	}

	if (!m_frame)
	{
		m_frame = av_frame_alloc();
		if (!m_frame)
		{
			return false;
		}
	}

	if (m_hw_available && !m_hw_frame)
	{
		m_hw_frame = av_frame_alloc();
		if (!m_hw_frame)
		{
			return false;
		}
	}

	ret = avcodec_send_packet(m_decoder_context, &packet);
	if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
	{
		return true;
	}
	else if (ret < 0)
	{
		return false;
	}

	return true;
}

AVFrame* FFmpegDecoder::receive_frame()
{
	AVFrame* retFrame = m_hw_available ? m_hw_frame : m_frame;
	int ret = avcodec_receive_frame(m_decoder_context, retFrame);
	if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
	{
		return NULL;
	}
	else if (ret < 0)
	{
		return NULL;
	}

	if (m_hw_available)
	{
		// retrieve data from GPU to CPU
		ret = av_hwframe_transfer_data(m_frame, m_hw_frame, 0);
		if (ret < 0)
		{
			return NULL;
		}
	}

	if (m_frame->format != AV_PIX_FMT_YUV420P)
	{
		if (scale_frame())
		{
			return m_sws_frame;
		}
	}
	else
	{
		return m_frame;
	}

	return NULL;
}

bool FFmpegDecoder::scale_frame()
{
	int ret;
	m_sws_context = sws_getCachedContext(m_sws_context,
		m_frame->width, m_frame->height, (AVPixelFormat)m_frame->format, m_frame->width, m_frame->height,
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
			ret = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, m_frame->width, m_frame->height, 1);
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
				AV_PIX_FMT_YUV420P, m_frame->width, m_frame->height, 1);
			if (ret < 0)
			{
				return false;
			}
		}

		sws_scale(m_sws_context, (const uint8_t * const *)m_frame->data, m_frame->linesize,
			0, m_frame->height, m_sws_frame->data, m_sws_frame->linesize);
		m_sws_frame->width = m_frame->width;
		m_sws_frame->height = m_frame->height;
		m_sws_frame->format = AV_PIX_FMT_YUV420P;
		return true;
	}
	else
	{
		return false;
	}
}