#ifndef _H_FFMPEG_DECODER_H_
#define _H_FFMPEG_DECODER_H_

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

/**
* ffmpeg decoder
*/
class FFmpegDecoder
{
public:
	FFmpegDecoder();
	virtual ~FFmpegDecoder();

	/**
	 * @brief if the decoder is initialized
	 *
	 * @return true -- initialized
	 *         false -- not initialized
	 */
	bool is_initialized() const
	{
		return m_initialized;
	}

	/**
	 * @brief initialize from the code id
	 * 
	 * @return true -- initialize successful
	 *         false -- initialize failed
	 */
	bool init(enum AVCodecID id);

	/**
	* @brief if the decoder supports codecid
	*
	* @return true
	*         false
	*/
	bool validate(enum AVCodecID id);

	/**
	* @brief send video data
	*
	* @param data -- the video data
	*        size -- the data size
	*        timestamp -- the timestamp
	*/
	bool send_video_data(uint8_t* data, size_t size, long long timestamp);

	/**
	* @brief receive the decoded frame
	* @return the AVFrame pointer, if failed, returns NULL.
	* Make sure that you MUST not delete the returned pointer,
	* it's lifetime was managed by the FFmpegDecoder.
	*
	*/
	AVFrame* receive_frame();

private:
	bool free_context();
	bool scale_frame();

	bool init_hw_decoder();
	bool has_hw_type(enum AVHWDeviceType type);
private:
	bool m_initialized;
	bool m_hw_available;
	AVCodecContext* m_decoder_context;
	AVCodec* m_decoder_codec;

	AVBufferRef* m_hw_ctx;
	AVFrame* m_hw_frame;
	AVFrame* m_frame;
	AVFrame* m_sws_frame;
	uint8_t* m_sws_frame_buffer;
	SwsContext* m_sws_context;
};

#endif
