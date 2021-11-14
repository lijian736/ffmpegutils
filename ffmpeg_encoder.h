#ifndef _H_FFMPEG_ENCODER_H_
#define _H_FFMPEG_ENCODER_H_

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include <libavutil/error.h>
#include <libavutil/hwcontext.h>
}

//#define USE_HARDWARE_ENCODER

//the encoder buffer size
constexpr int ENCODER_BUFFER_SIZE = 1024 * 256;

/**
* ffmpeg encoder
*/
class FFmpegEncoder
{
public:
	FFmpegEncoder();
	virtual ~FFmpegEncoder();

	bool is_initialized()const
	{
		return m_initialized;
	}

	/**
	 * initialize
	 * @param width -- the source yuv image width
	 *        height -- the source yuv image height
	 *        pixelFormat -- the source data pixel format, now it must be AV_PIX_FMT_YUV420P
	 */
	bool init(int width, int height, AVPixelFormat pixelFormat);

	/**
	* encode the data to h264
	* @param width -- [input]the image width
	*        height -- [input]the image height
	*        data -- [input]the data
	*        linesize -- [input]the line size
	*/
	bool send_video_data(int width, int height, uint8_t* data[], int linesize[]);

	/**
	 * receive the encoded packet
	 * @return the AVPakcet pointer, if failed, returns NULL.
	 * Make sure that you MUST not delete the returned pointer,
	 * it's lifetime was managed by the FFmpegEncoder.
	 */
	AVPacket* receive_packet();
	void end_receive_packet();

	/**
	 * receive the encoded packets data
	 * @param data -- output parameter, the data pointer reference
	 *        len -- output parameter, the data length
	 * @return true - successful, false - failed
	 */
	bool receive_packets(uint8_t*& data, size_t& len);
private:
	bool free_context();

#ifdef USE_HARDWARE_ENCODER
	int set_hwframe_ctx(int width, int height);
#endif

private:
	bool m_initialized;

#ifdef USE_HARDWARE_ENCODER
	bool m_hw_available;
	AVBufferRef* m_hw_ctx;
	AVFrame* m_hw_frame;
#endif

	AVCodecContext* m_encoder_context;
	AVCodec* m_encoder_codec;

	AVPacket* m_packet;
	AVFrame* m_frame;
	int64_t m_pts;

	uint8_t* m_buffer;
	size_t m_buffer_used_len;
};

#endif