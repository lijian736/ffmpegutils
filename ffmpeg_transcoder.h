#ifndef _H_FFMPEG_TRANSCODER_H_
#define _H_FFMPEG_TRANSCODER_H_

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h> 
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

/**
* the ffmpeg transcoder
*/
class FFmpegTranscoder
{
public:
	FFmpegTranscoder();
	virtual ~FFmpegTranscoder();

	/**
	* @brief Scale video data
	*
	* @param data -- [input] the video data
	*        linesize -- [input] the data linesize
	*        width -- [input] the image width
	*        height -- [input] the image height
	*        format -- [input] the ffmpeg format of image
	*        frame -- [output] the transcoded frame
	*
	* @return true -- transcode successful
	*         false -- transcode failed
	*/
	bool scale_yuv(uint8_t *data, int* linesize, int width, int height, AVPixelFormat format, AVFrame** frame);

private:
	void free_context();

private:
	SwsContext* m_sws_context;
	uint8_t* m_sws_frame_buffer;
	AVFrame *m_sws_frame;

	AVPixelFormat m_src_pixel_format;
	int m_src_width;
	int m_src_height;
};

#endif