#include "ffmpeg_encoder.h"
#include <new>

namespace
{
#ifdef USE_HARDWARE_ENCODER
	//hardware encoder name
	const char *g_enc_name = "h264_vaapi";  //h264_qsv, h264_nvenc, h264_vaapi, h264_dxva2
	enum AVHWDeviceType g_hw_device_type = AV_HWDEVICE_TYPE_VAAPI; //AV_HWDEVICE_TYPE_DXVA2, AV_HWDEVICE_TYPE_QSV, AV_HWDEVICE_TYPE_VAAPI, AV_HWDEVICE_TYPE_CUDA
	AVPixelFormat g_hw_pixel_format = AV_PIX_FMT_VAAPI; //AV_PIX_FMT_DXVA2_VLD,  AV_PIX_FMT_VAAPI, AV_PIX_FMT_CUDA, AV_PIX_FMT_QSV
#endif

	//get the milliseconds
	static int64_t GetTime()
	{
		return av_gettime_relative() / 1000;
	}

}
FFmpegEncoder::FFmpegEncoder()
{
	m_encoder_context = NULL;
	m_encoder_codec = NULL;
	m_frame = NULL;
	m_packet = NULL;

	m_buffer = NULL;
	m_buffer_used_len = 0;

#ifdef USE_HARDWARE_ENCODER
	m_hw_ctx = NULL;
	m_hw_frame = NULL;
	m_hw_available = false;
#endif

	m_pts = 0;
	
	m_initialized = false;
}

FFmpegEncoder::~FFmpegEncoder()
{
	free_context();
}

bool FFmpegEncoder::init(int width, int height, AVPixelFormat pixelFormat)
{
	int ret;

	free_context();

	if (width <= 0 || height <= 0)
	{
		return false;
	}

	m_buffer = new (std::nothrow) uint8_t[ENCODER_BUFFER_SIZE];
	if (!m_buffer)
	{
		return false;
	}

#ifdef USE_HARDWARE_ENCODER
	ret = av_hwdevice_ctx_create(&m_hw_ctx, g_hw_device_type, NULL, NULL, 0);
	if (ret == 0)
	{
		m_hw_available = true;

		m_encoder_codec = avcodec_find_encoder_by_name(g_enc_name);
		if (!m_encoder_codec)
		{
			m_hw_available = false;
			// find encoder
			m_encoder_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
		}
	}
	else
	{
		char errStr[512] = { 0 };
		av_strerror(ret, errStr, sizeof(errStr));

		m_hw_available = false;
		m_encoder_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	}

#else
	m_encoder_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
#endif

	if (!m_encoder_codec)
	{
		return false;
	}

	// get the encoder context
	m_encoder_context = avcodec_alloc_context3(m_encoder_codec);
	if (!m_encoder_context)
	{
		return false;
	}

	// alloc packet and frame
	m_packet = av_packet_alloc();
	if (!m_packet)
	{
		return false;
	}

	// set encoder parameters
	// set resolution, it must be a multiple of two
	m_encoder_context->width = width;
	m_encoder_context->height = height;
	// frames per second
	m_encoder_context->time_base.num = 1;
	m_encoder_context->time_base.den = 25;
	m_encoder_context->framerate.num = 25;
	m_encoder_context->framerate.den = 1;
	m_encoder_context->qmin = 10;
	m_encoder_context->qmax = 51;

	// if frame->pict_type is AV_PICTURE_TYPE_I, then gop_size is ignored and
	// the output of encoder will always be I frame irrespective to gop_size.
	// I frame interval
	m_encoder_context->gop_size = 25;
	// if you don't need b frame, then set to 0
	m_encoder_context->max_b_frames = 0;
	m_encoder_context->pix_fmt = pixelFormat;
	// put sample parameters
	m_encoder_context->bit_rate = 400000;

	ret = av_opt_set(m_encoder_context->priv_data, "preset", "ultrafast", 0);  //default value is slow. ultrafast、superfast、veryfast、faster、fast、medium、slow、slower、veryslow、placebo
	if (ret != 0)
	{
	}
	ret = av_opt_set(m_encoder_context->priv_data, "profile", "baseline", 0); // baseline、main、high、high10、high422、high444
	if (ret != 0)
	{
	}
	ret = av_opt_set(m_encoder_context->priv_data, "tune", "zerolatency", 0);
	//ret = av_opt_set(m_encoder_context->priv_data, "tune", "film", 0); //  film, animation, grain, stillimage, psnr, ssim, fastdecode, zerolatency
	//if (ret != 0)
	//{
	//	LOG_ERROR("av_opt_set tune failed");
	//}

#ifdef USE_HARDWARE_ENCODER
	if (m_hw_available)
	{
		ret = set_hwframe_ctx(width, height);
		if (ret < 0)
		{
			m_hw_available = false;
		}

		if (m_hw_available)
		{
		//	m_encoder_context->pix_fmt = g_hw_pixel_format;
		}
	}
#endif

	// if H264 AV_CODEC_FLAG_GLOBAL_HEADER was set,
	// the sps, pps, sei is in encoderContext->extradata.
	// otherwise, each I frame will contais sps, pps, sei
	// if the data was save in local file, it's better not to set AV_CODEC_FLAG_GLOBAL_HEADER
	//encoderContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	// open
	ret = avcodec_open2(m_encoder_context, m_encoder_codec, NULL);
	if (ret < 0)
	{
		char errStr[512] = { 0 };
		av_strerror(ret, errStr, sizeof(errStr));

		return false;
	}

	m_frame = av_frame_alloc();
	if (!m_frame)
	{
		return false;
	}
	// Allocate new buffer(s) for audio or video data.
	m_frame->format = pixelFormat;
	m_frame->width = m_encoder_context->width;
	m_frame->height = m_encoder_context->height;

	/**
	ret = av_frame_get_buffer(frame, 0);
	if (ret < 0)
	{
		LOG_ERROR("could not allocate the video frame data");
		return false;
	}
	*/

#ifdef USE_HARDWARE_ENCODER
	if (m_hw_available)
	{
		if (!(m_hw_frame = av_frame_alloc())) 
		{
			return false;
		}

		if ((ret = av_hwframe_get_buffer(m_encoder_context->hw_frames_ctx, m_hw_frame, 0)) < 0) 
		{
			return false;
		}

		if (!m_hw_frame->hw_frames_ctx) 
		{
			return false;
		}
	}

#endif

	m_pts = 0;
	m_initialized = true;
	return true;
}

#ifdef USE_HARDWARE_ENCODER
int FFmpegEncoder::set_hwframe_ctx(int width, int height)
{
	AVBufferRef *hw_frames_ref;
	AVHWFramesContext *frames_ctx = NULL;
	int err = 0;

	if (!(hw_frames_ref = av_hwframe_ctx_alloc(m_hw_ctx)))
	{
		return -1;
	}

	frames_ctx = (AVHWFramesContext *)(hw_frames_ref->data);
	frames_ctx->format = g_hw_pixel_format;
	frames_ctx->sw_format = AV_PIX_FMT_YUV420P;
	frames_ctx->width = width;
	frames_ctx->height = height;
	frames_ctx->initial_pool_size = 20;

	if ((err = av_hwframe_ctx_init(hw_frames_ref)) < 0)
	{
		av_buffer_unref(&hw_frames_ref);
		return err;
	}

	m_encoder_context->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
	if (!m_encoder_context->hw_frames_ctx)
	{
		err = AVERROR(ENOMEM);
	}

	av_buffer_unref(&hw_frames_ref);
	return err;
}
#endif

bool FFmpegEncoder::send_video_data(int width, int height, uint8_t* data_p[], int linesize_p[])
{
	if (!m_initialized)
	{
		return false;
	}

	m_frame->pts = m_pts++;
	for (int i = 0; i < 3; i++)
	{
		m_frame->data[i] = data_p[i];
		m_frame->linesize[i] = linesize_p[i];
	}

	int err;
#ifdef USE_HARDWARE_ENCODER
	if (m_hw_available)
	{
		if ((err = av_hwframe_transfer_data(m_hw_frame, m_frame, 0)) < 0) 
		{
			return false;
		}

		err = avcodec_send_frame(m_encoder_context, m_hw_frame);
		if (err < 0)
		{
			return false;
		}
	}
	else
	{
		err = avcodec_send_frame(m_encoder_context, m_frame);
		if (err < 0)
		{
			return false;
		}
	}
#else
	err = avcodec_send_frame(m_encoder_context, m_frame);
	if (err < 0)
	{
		return false;
	}
#endif

	return true;
}

AVPacket* FFmpegEncoder::receive_packet()
{
	int ret = avcodec_receive_packet(m_encoder_context, m_packet);
	if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
	{
		return NULL;
	}
	else if (ret < 0)
	{
		return NULL;
	}

	return m_packet;
}

void FFmpegEncoder::end_receive_packet()
{
	av_packet_unref(m_packet);
}

bool FFmpegEncoder::receive_packets(uint8_t*& data, size_t& len)
{
	m_buffer_used_len = 0;

	AVPacket* packet;
	while ((packet = this->receive_packet()) != NULL)
	{
		if (m_buffer_used_len + packet->size > ENCODER_BUFFER_SIZE)
		{
			return false;
		}

		memcpy(m_buffer + m_buffer_used_len, packet->data, packet->size);
		m_buffer_used_len += packet->size;

		this->end_receive_packet();
	}

	data = m_buffer;
	len = m_buffer_used_len;

	return true;
}

bool FFmpegEncoder::free_context()
{
	if (m_frame)
	{
		av_frame_free(&m_frame);
		m_frame = NULL;
	}

	if (m_packet)
	{
		av_packet_free(&m_packet);
		m_packet = NULL;
	}

	if (m_encoder_context)
	{
		avcodec_free_context(&m_encoder_context);
		m_encoder_context = NULL;
	}

#ifdef USE_HARDWARE_ENCODER
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

	m_hw_available = false;
#endif

	if (m_buffer)
	{
		delete[] m_buffer;
		m_buffer = NULL;
	}

	m_pts = 0;
	m_initialized = false;

	return true;
}