/*
 * Copyright (c) 2010 Nicolas George
 * Copyright (c) 2011 Stefano Sabatini
 * Copyright (c) 2014 Andrey Utkin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

 /**
  * @file
  * API example for demuxing, decoding, filtering, encoding and muxing
  * @example transcoding.c
  */


using namespace std;

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libavfilter/buffersink.h>
	#include <libavfilter/buffersrc.h>
	#include <libavutil/opt.h>
	#include <libavutil/pixdesc.h>
	#include <libavutil/rational.h>

}

class Encoder {

public:
	typedef struct FilteringContext;
	typedef struct StreamContext;

	static AVFormatContext* ifmt_ctx;
	static AVFormatContext* ofmt_ctx_1;
	static AVFormatContext* ofmt_ctx_2;
	static AVFormatContext* ofmt_ctx_3;
	static AVFormatContext* ofmt_ctx_4;
	static FilteringContext* filter_ctx[4];
	
	static StreamContext* stream_ctx;

	static int open_input_file(const char* filename);
	static int open_output_file(const char* filename1, const char* filename2, const char* filename3, const char* filename4);
	static int init_filter(FilteringContext* fctx, AVCodecContext* dec_ctx, AVCodecContext* enc_ctx_1, const char* filter_spec);
	static int init_filters(int);
	static int encode_write_frame(AVFrame* filt_frame, unsigned int stream_index, int* got_frame, int encodNum);
	static int filter_encode_write_frame(AVFrame* frame, unsigned int stream_index, int encoderNum);
	static int flush_encoder(unsigned int stream_index);

};

struct Encoder:: FilteringContext {
	AVFilterContext* buffersink_ctx;
	AVFilterContext* buffersrc_ctx;
	AVFilterGraph* filter_graph;
}FilteringContext1,FilteringContext2, FilteringContext3, FilteringContext4;

struct Encoder::StreamContext {
	AVCodecContext* dec_ctx;
	AVCodecContext* enc_ctx_1;
	AVCodecContext* enc_ctx_2;
	AVCodecContext* enc_ctx_3;
	AVCodecContext* enc_ctx_4;
} StreamContext;

AVFormatContext* Encoder::ifmt_ctx = NULL;
AVFormatContext* Encoder::ofmt_ctx_1 = NULL;
AVFormatContext* Encoder::ofmt_ctx_2 = NULL;
AVFormatContext* Encoder::ofmt_ctx_3 = NULL;
AVFormatContext* Encoder::ofmt_ctx_4 = NULL;

struct Encoder::FilteringContext* Encoder::filter_ctx[] = { &FilteringContext1, &FilteringContext2, &FilteringContext3, &FilteringContext4 };
struct Encoder::StreamContext* Encoder::stream_ctx = NULL;



int  Encoder::open_input_file(const char* filename)
{
	int ret;
	unsigned int i;

	ifmt_ctx = NULL;
	if ((ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL)) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
		return ret;
	}

	if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
		return ret;
	}

	stream_ctx = (StreamContext*)av_mallocz_array(ifmt_ctx->nb_streams, sizeof(*stream_ctx));
	if (!stream_ctx)
		return AVERROR(ENOMEM);

	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		AVStream* stream = ifmt_ctx->streams[i];
		AVCodec* dec = avcodec_find_decoder(stream->codecpar->codec_id);
		AVCodecContext* codec_ctx;
		if (!dec) {
			av_log(NULL, AV_LOG_ERROR, "Failed to find decoder for stream #%u\n", i);
			return AVERROR_DECODER_NOT_FOUND;
		}
		codec_ctx = avcodec_alloc_context3(dec);
		if (!codec_ctx) {
			av_log(NULL, AV_LOG_ERROR, "Failed to allocate the decoder context for stream #%u\n", i);
			return AVERROR(ENOMEM);
		}
		ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Failed to copy decoder parameters to input decoder context "
				"for stream #%u\n", i);
			return ret;
		}
		/* Reencode video & audio and remux subtitles etc. */
		if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
			|| codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
			if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
				codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx, stream, NULL);
			/* Open decoder */
			ret = avcodec_open2(codec_ctx, dec, NULL);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
				return ret;
			}
		}
		stream_ctx[i].dec_ctx = codec_ctx;
	}

	av_dump_format(ifmt_ctx, 0, filename, 0);
	return 0;
}

int Encoder:: open_output_file(const char* filename1, const char* filename2, const char* filename3, const char* filename4)
{
	AVStream* out_stream_1;
	AVStream* out_stream_2;
	AVStream* out_stream_3;
	AVStream* out_stream_4;
	AVStream* in_stream;
	AVCodecContext* dec_ctx, *enc_ctx_1, *enc_ctx_2, *enc_ctx_3, *enc_ctx_4;
	AVCodec* encoder;
	int ret;
	unsigned int i;

	


	//ofmt_ctx_1 = NULL;

	/*Allocate avformat output context for 4 different file names*/
	avformat_alloc_output_context2(&ofmt_ctx_1, NULL, NULL, filename1);
	if (!ofmt_ctx_1) {
		av_log(NULL, AV_LOG_ERROR, "Could not create output context_1\n");
		return AVERROR_UNKNOWN;
	}
	avformat_alloc_output_context2(&ofmt_ctx_2, NULL, NULL, filename2);
	if (!ofmt_ctx_1) {
		av_log(NULL, AV_LOG_ERROR, "Could not create output context_2\n");
		return AVERROR_UNKNOWN;
	}

	avformat_alloc_output_context2(&ofmt_ctx_3, NULL, NULL, filename3);
	if (!ofmt_ctx_1) {
		av_log(NULL, AV_LOG_ERROR, "Could not create output context_3\n");
		return AVERROR_UNKNOWN;
	}

	avformat_alloc_output_context2(&ofmt_ctx_4, NULL, NULL, filename4);
	if (!ofmt_ctx_1) {
		av_log(NULL, AV_LOG_ERROR, "Could not create output context_4\n");
		return AVERROR_UNKNOWN;
	}

	
	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		
		/*Adding 4 output stream for the 4 seperate files*/
		out_stream_1 = avformat_new_stream(ofmt_ctx_1, NULL);
		if (!out_stream_1) {
			av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream_1\n");
			return AVERROR_UNKNOWN;
		}

		out_stream_2 = avformat_new_stream(ofmt_ctx_2, NULL);
		if (!out_stream_2) {
			av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream_1\n");
			return AVERROR_UNKNOWN;
		}
		out_stream_3 = avformat_new_stream(ofmt_ctx_3, NULL);
		if (!out_stream_3) {
			av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream_1\n");
			return AVERROR_UNKNOWN;
		}
		out_stream_4 = avformat_new_stream(ofmt_ctx_4, NULL);
		if (!out_stream_4) {
			av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream_1\n");
			return AVERROR_UNKNOWN;
		}


		in_stream = ifmt_ctx->streams[i];
		dec_ctx = stream_ctx[i].dec_ctx;

		if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO || dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
			/* in this example, we choose transcoding to same codec */
			encoder = avcodec_find_encoder(AV_CODEC_ID_H265);
			if (!encoder) {
				av_log(NULL, AV_LOG_FATAL, "Necessary encoder not found\n");
				return AVERROR_INVALIDDATA;
			}
			/*Allocating context for 4 other encoder contexts*/
			enc_ctx_1 = avcodec_alloc_context3(encoder);
			enc_ctx_2 = avcodec_alloc_context3(encoder);
			enc_ctx_3 = avcodec_alloc_context3(encoder);
			enc_ctx_4 = avcodec_alloc_context3(encoder);
			/*===============================================*/

			/*Validate whther 4 encode context are created or not*/
			if (!enc_ctx_1) {
				av_log(NULL, AV_LOG_FATAL, "Failed to allocate the encoder context\n");
				return AVERROR(ENOMEM);
			}
			if (!enc_ctx_2) {
				av_log(NULL, AV_LOG_FATAL, "Failed to allocate the encoder context\n");
				return AVERROR(ENOMEM);
			}
			if (!enc_ctx_3) {
				av_log(NULL, AV_LOG_FATAL, "Failed to allocate the encoder context\n");
				return AVERROR(ENOMEM);
			}
			if (!enc_ctx_4) {
				av_log(NULL, AV_LOG_FATAL, "Failed to allocate the encoder context\n");
				return AVERROR(ENOMEM);
			}
			/*====================================================*/

			/* In this example, we transcode to same properties (picture size,
			 * sample rate etc.). These properties can be changed for output
			 * streams easily using filters */
			if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {



				/*enc_ctx_1->bit_rate = 88000;
				enc_ctx_2->bit_rate = 88000;
				enc_ctx_3->bit_rate = 88000;
				enc_ctx_4->bit_rate = 88000;

				enc_ctx_1->rc_min_rate = 90000;
				enc_ctx_2->rc_min_rate = 90000;
				enc_ctx_3->rc_min_rate = 90000;
				enc_ctx_4->rc_min_rate = 90000;

				enc_ctx_1->rc_max_rate = 86000;
				enc_ctx_2->rc_max_rate = 86000;
				enc_ctx_3->rc_max_rate = 86000;
				enc_ctx_4->rc_max_rate = 86000;*/



				enc_ctx_1->height = dec_ctx->height; 
				enc_ctx_2->height = dec_ctx->height;
				enc_ctx_3->height = dec_ctx->height;
				enc_ctx_4->height = dec_ctx->height;
				
				enc_ctx_1->width = dec_ctx->width; 
				enc_ctx_2->width = dec_ctx->width;
				enc_ctx_3->width = dec_ctx->width;
				enc_ctx_4->width = dec_ctx->width;

				enc_ctx_1->sample_aspect_ratio = dec_ctx->sample_aspect_ratio; 
				enc_ctx_2->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
				enc_ctx_3->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
				enc_ctx_4->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;

				//enc_ct
				
				/* take first format from list of supported formats */
				//if (encoder->pix_fmts) 
					enc_ctx_1->pix_fmt = AV_PIX_FMT_YUV420P;
					enc_ctx_2->pix_fmt = AV_PIX_FMT_YUV420P;
					enc_ctx_3->pix_fmt = AV_PIX_FMT_YUV420P;
					enc_ctx_4->pix_fmt = AV_PIX_FMT_YUV420P;
				
					
				/*else {
					enc_ctx_1->pix_fmt = dec_ctx->pix_fmt;
					enc_ctx_2->pix_fmt = dec_ctx->pix_fmt;
					enc_ctx_3->pix_fmt = dec_ctx->pix_fmt;
					enc_ctx_4->pix_fmt = dec_ctx->pix_fmt;
				}*/
					
				/* video time_base can be set to whatever is handy and supported by encoder */
				AVRational av1 = { 1,60};
				enc_ctx_1->time_base = av1;
				enc_ctx_2->time_base = av1;
				enc_ctx_3->time_base = av1;
				enc_ctx_4->time_base = av1;
				//enc_ctx_1->time_base = av_inv_q(dec_ctx->framerate);

				enc_ctx_1->gop_size = dec_ctx->framerate.num;
				enc_ctx_2->gop_size = dec_ctx->framerate.num;
				enc_ctx_3->gop_size = dec_ctx->framerate.num;
				enc_ctx_4->gop_size = dec_ctx->framerate.num;

				//enc_ctx_1->frame_skip_factor = 3;
				

				
				/*AVRational av3 = { dec_ctx->framerate.num / 4,dec_ctx->framerate.den };
				enc_ctx_1->framerate = av3;
				enc_ctx_2->framerate = av3;
				enc_ctx_3->framerate = av3;
				enc_ctx_4->framerate = av3;*/

				av_opt_set(enc_ctx_1->priv_data, "preset", "slow", 0);
				av_opt_set(enc_ctx_2->priv_data, "preset", "slow", 0);
				av_opt_set(enc_ctx_3->priv_data, "preset", "slow", 0);
				av_opt_set(enc_ctx_4->priv_data, "preset", "slow", 0);

			}
			else {
				enc_ctx_1->sample_rate = dec_ctx->sample_rate;
				enc_ctx_2->sample_rate = dec_ctx->sample_rate;
				enc_ctx_3->sample_rate = dec_ctx->sample_rate;
				enc_ctx_4->sample_rate = dec_ctx->sample_rate;

				enc_ctx_1->channel_layout = dec_ctx->channel_layout;
				enc_ctx_2->channel_layout = dec_ctx->channel_layout;
				enc_ctx_3->channel_layout = dec_ctx->channel_layout;
				enc_ctx_4->channel_layout = dec_ctx->channel_layout;

				enc_ctx_1->channels = av_get_channel_layout_nb_channels(enc_ctx_1->channel_layout);
				enc_ctx_2->channels = av_get_channel_layout_nb_channels(enc_ctx_2->channel_layout);
				enc_ctx_3->channels = av_get_channel_layout_nb_channels(enc_ctx_3->channel_layout);
				enc_ctx_4->channels = av_get_channel_layout_nb_channels(enc_ctx_4->channel_layout);

				/* take first format from list of supported formats */
				enc_ctx_1->sample_fmt = encoder->sample_fmts[0];
				enc_ctx_2->sample_fmt = encoder->sample_fmts[0];
				enc_ctx_3->sample_fmt = encoder->sample_fmts[0];
				enc_ctx_4->sample_fmt = encoder->sample_fmts[0];

				AVRational av2 = { 1, enc_ctx_1->sample_rate };

				enc_ctx_1->time_base = av2;
				enc_ctx_2->time_base = av2;
				enc_ctx_3->time_base = av2;
				enc_ctx_4->time_base = av2;
			}

			/*Since all the 4 streams are the same we can 
			just consider the ofmt_ctx_1*/
			if (ofmt_ctx_1->oformat->flags & AVFMT_GLOBALHEADER) {
				enc_ctx_1->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
				enc_ctx_2->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
				enc_ctx_3->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
				enc_ctx_4->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
			}
				

			/* Third parameter can be used to pass settings to encoder */
			ret = avcodec_open2(enc_ctx_1, encoder, NULL);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Cannot open video encoder for stream enc_ctx_1 #%u\n", i);
				return ret;
			}
			ret = avcodec_open2(enc_ctx_2, encoder, NULL);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Cannot open video encoder for stream enc_ctx_2 #%u\n", i);
				return ret;
			}
			ret = avcodec_open2(enc_ctx_3, encoder, NULL);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Cannot open video encoder for stream enc_ctx_3 #%u\n", i);
				return ret;
			}
			ret = avcodec_open2(enc_ctx_4, encoder, NULL);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Cannot open video encoder for stream enc_ctx_4 #%u\n", i);
				return ret;
			}
			/*==========================================================*/


			/*Copying the encoder parameter to the 4 output streams*/
			ret = avcodec_parameters_from_context(out_stream_1->codecpar, enc_ctx_1);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Failed to copy encoder parameters to output stream 1 #%u\n", i);
				return ret;
			}
			ret = avcodec_parameters_from_context(out_stream_2->codecpar, enc_ctx_2);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Failed to copy encoder parameters to output stream 2 #%u\n", i);
				return ret;
			}
			ret = avcodec_parameters_from_context(out_stream_3->codecpar, enc_ctx_3);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Failed to copy encoder parameters to output stream 3 #%u\n", i);
				return ret;
			}
			ret = avcodec_parameters_from_context(out_stream_4->codecpar, enc_ctx_4);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Failed to copy encoder parameters to output stream 4 #%u\n", i);
				return ret;
			}
			/*======================================================*/

			
			out_stream_1->time_base = enc_ctx_1->time_base;
			out_stream_2->time_base = enc_ctx_2->time_base;
			out_stream_3->time_base = enc_ctx_3->time_base;
			out_stream_4->time_base = enc_ctx_4->time_base;

			stream_ctx[i].enc_ctx_1 = enc_ctx_1;
			stream_ctx[i].enc_ctx_2 = enc_ctx_2;
			stream_ctx[i].enc_ctx_3 = enc_ctx_3;
			stream_ctx[i].enc_ctx_4 = enc_ctx_4;

		}
		else if (dec_ctx->codec_type == AVMEDIA_TYPE_UNKNOWN) {
			av_log(NULL, AV_LOG_FATAL, "Elementary stream #%d is of unknown type, cannot proceed\n", i);
			return AVERROR_INVALIDDATA;
		}
		else {
			/* if this stream must be remuxed */
			ret = avcodec_parameters_copy(out_stream_1->codecpar, in_stream->codecpar);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Copying parameters for stream 1 #%u failed\n", i);
				return ret;
			}
			ret = avcodec_parameters_copy(out_stream_2->codecpar, in_stream->codecpar);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Copying parameters for stream 2 #%u failed\n", i);
				return ret;
			}
			ret = avcodec_parameters_copy(out_stream_3->codecpar, in_stream->codecpar);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Copying parameters for stream 3 #%u failed\n", i);
				return ret;
			}
			ret = avcodec_parameters_copy(out_stream_4->codecpar, in_stream->codecpar);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Copying parameters for stream 4 #%u failed\n", i);
				return ret;
			}
			out_stream_1->time_base = in_stream->time_base;
			out_stream_2->time_base = in_stream->time_base;
			out_stream_3->time_base = in_stream->time_base;
			out_stream_4->time_base = in_stream->time_base;
		}

	}
	av_dump_format(ofmt_ctx_1, 0, filename1, 1);
	av_dump_format(ofmt_ctx_2, 0, filename2, 1);
	av_dump_format(ofmt_ctx_3, 0, filename3, 1);
	av_dump_format(ofmt_ctx_4, 0, filename4, 1);

	if (!(ofmt_ctx_1->oformat->flags & AVFMT_NOFILE)) {
		ret = avio_open(&ofmt_ctx_1->pb, filename1, AVIO_FLAG_WRITE);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Could not open output file 1 '%s'", filename1);
			return ret;
		}
	}
	if (!(ofmt_ctx_2->oformat->flags & AVFMT_NOFILE)) {
		ret = avio_open(&ofmt_ctx_2->pb, filename2, AVIO_FLAG_WRITE);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Could not open output file 2 '%s'", filename2);
			return ret;
		}
	}
	if (!(ofmt_ctx_3->oformat->flags & AVFMT_NOFILE)) {
		ret = avio_open(&ofmt_ctx_3->pb, filename3, AVIO_FLAG_WRITE);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Could not open output file 3 '%s'", filename3);
			return ret;
		}
	}
	if (!(ofmt_ctx_4->oformat->flags & AVFMT_NOFILE)) {
		ret = avio_open(&ofmt_ctx_4->pb, filename4, AVIO_FLAG_WRITE);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Could not open output file 4'%s'", filename4);
			return ret;
		}
	}

	/* init muxer, write output file header */
	ret = avformat_write_header(ofmt_ctx_1, NULL);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output 1 file\n");
		return ret;
	}
	ret = avformat_write_header(ofmt_ctx_2, NULL);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output 2 file\n");
		return ret;
	}

	ret = avformat_write_header(ofmt_ctx_3, NULL);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output 3 file\n");
		return ret;
	}

	ret = avformat_write_header(ofmt_ctx_4, NULL);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output 4 file\n");
		return ret;
	}
	return 0;
}

int Encoder::init_filter(FilteringContext* fctx, AVCodecContext* dec_ctx, AVCodecContext* enc_ctx, const char* filter_spec)
{
	char args[512];
	int ret = 0;
	const AVFilter* buffersrc = NULL;
	const AVFilter* buffersink = NULL;
	AVFilterContext* buffersrc_ctx = NULL;
	AVFilterContext* buffersink_ctx = NULL;
	AVFilterInOut* outputs = avfilter_inout_alloc();
	AVFilterInOut* inputs = avfilter_inout_alloc();
	AVFilterGraph* filter_graph = avfilter_graph_alloc();

	if (!outputs || !inputs || !filter_graph) {
		ret = AVERROR(ENOMEM);
		goto end;
	}

	if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
		buffersrc = avfilter_get_by_name("buffer");
		buffersink = avfilter_get_by_name("buffersink");
		if (!buffersrc || !buffersink) {
			av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
			ret = AVERROR_UNKNOWN;
			goto end;
		}

		snprintf(args, sizeof(args),
			"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
			dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
			dec_ctx->time_base.num, dec_ctx->time_base.den,
			dec_ctx->sample_aspect_ratio.num,
			dec_ctx->sample_aspect_ratio.den);

		ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, NULL, filter_graph);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
			goto end;
		}

		ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", NULL, NULL, filter_graph);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
			goto end;
		}

		ret = av_opt_set_bin(buffersink_ctx, "pix_fmts",(uint8_t*)& enc_ctx->pix_fmt, sizeof(enc_ctx->pix_fmt),
			AV_OPT_SEARCH_CHILDREN);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
			goto end;
		}
	}
	else if (dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
		buffersrc = avfilter_get_by_name("abuffer");
		buffersink = avfilter_get_by_name("abuffersink");
		if (!buffersrc || !buffersink) {
			av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
			ret = AVERROR_UNKNOWN;
			goto end;
		}

		if (!dec_ctx->channel_layout)
			dec_ctx->channel_layout =
			av_get_default_channel_layout(dec_ctx->channels);
		snprintf(args, sizeof(args),
			/*changed*/
			"time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%" "llx",
			dec_ctx->time_base.num, dec_ctx->time_base.den, dec_ctx->sample_rate,
			av_get_sample_fmt_name(dec_ctx->sample_fmt),
			dec_ctx->channel_layout);
		ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
			args, NULL, filter_graph);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
			goto end;
		}

		ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
			NULL, NULL, filter_graph);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
			goto end;
		}

		ret = av_opt_set_bin(buffersink_ctx, "sample_fmts",
			(uint8_t*)& enc_ctx->sample_fmt, sizeof(enc_ctx->sample_fmt),
			AV_OPT_SEARCH_CHILDREN);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
			goto end;
		}

		ret = av_opt_set_bin(buffersink_ctx, "channel_layouts",
			(uint8_t*)& enc_ctx->channel_layout,
			sizeof(enc_ctx->channel_layout), AV_OPT_SEARCH_CHILDREN);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot set output channel layout\n");
			goto end;
		}

		ret = av_opt_set_bin(buffersink_ctx, "sample_rates",
			(uint8_t*)& enc_ctx->sample_rate, sizeof(enc_ctx->sample_rate),
			AV_OPT_SEARCH_CHILDREN);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot set output sample rate\n");
			goto end;
		}
	}
	else {
		ret = AVERROR_UNKNOWN;
		goto end;
	}

	/* Endpoints for the filter graph. */
	outputs->name = av_strdup("in");
	outputs->filter_ctx = buffersrc_ctx;
	outputs->pad_idx = 0;
	outputs->next = NULL;

	inputs->name = av_strdup("out");
	inputs->filter_ctx = buffersink_ctx;
	inputs->pad_idx = 0;
	inputs->next = NULL;

	if (!outputs->name || !inputs->name) {
		ret = AVERROR(ENOMEM);
		goto end;
	}

	if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec, &inputs, &outputs, NULL)) < 0)
		goto end;

	if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
		goto end;

	/* Fill FilteringContext */
	fctx->buffersrc_ctx = buffersrc_ctx;
	fctx->buffersink_ctx = buffersink_ctx;
	fctx->filter_graph = filter_graph;

end:
	avfilter_inout_free(&inputs);
	avfilter_inout_free(&outputs);

	return ret;
}

int Encoder:: init_filters(int encoderNum)
{
	const char* filter_spec;
	unsigned int i;
	int ret;
	filter_ctx[encoderNum] = (FilteringContext*)av_malloc_array(ifmt_ctx->nb_streams, sizeof(*filter_ctx));
	if (!filter_ctx)
		return AVERROR(ENOMEM);

	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		filter_ctx[encoderNum][i].buffersrc_ctx = NULL;
		filter_ctx[encoderNum][i].buffersink_ctx = NULL;
		filter_ctx[encoderNum][i].filter_graph = NULL;
		if (!(ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO || ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO))
			continue;


		if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
			filter_spec = "null"; /* passthrough (dummy) filter for video */
		else
			filter_spec = "anull"; /* passthrough (dummy) filter for audio */

		/*Initialize 4 filters seperately for 4 encoders*/
		ret = -1;
		switch (encoderNum) {
		case 0:ret = init_filter(&filter_ctx[encoderNum][i], stream_ctx[i].dec_ctx, stream_ctx[i].enc_ctx_1, filter_spec);
			break;
		case 1:ret = init_filter(&filter_ctx[encoderNum][i], stream_ctx[i].dec_ctx, stream_ctx[i].enc_ctx_2, filter_spec);
			break;
		case 2:ret = init_filter(&filter_ctx[encoderNum][i], stream_ctx[i].dec_ctx, stream_ctx[i].enc_ctx_3, filter_spec);
			break;
		case 3:ret = init_filter(&filter_ctx[encoderNum][i], stream_ctx[i].dec_ctx, stream_ctx[i].enc_ctx_4, filter_spec);
			break;
		}
		
		if (ret)
			return ret;

	}
	return 0;
}

int Encoder::encode_write_frame(AVFrame* filt_frame, unsigned int stream_index, int* got_frame, int encodNum) {
	int ret = -1;
	int got_frame_local;
	AVPacket enc_pkt;
	int(*enc_func)(AVCodecContext*, AVPacket*, const AVFrame*, int*) = (ifmt_ctx->streams[stream_index]->codecpar->codec_type ==AVMEDIA_TYPE_VIDEO) ? avcodec_encode_video2 : avcodec_encode_audio2;

	if (!got_frame)
		got_frame = &got_frame_local;


	av_log(NULL, AV_LOG_INFO, "Encoding frame\n");
	/* encode filtered frame */
	enc_pkt.data = NULL;
	enc_pkt.size = 0;
	av_init_packet(&enc_pkt);

	//if (filt_frame)
		//printf("Send frame %3"PRId64"\n", filt_frame->pts);
	switch (encodNum) {
	case 0: ret = enc_func(stream_ctx[stream_index].enc_ctx_1, &enc_pkt, filt_frame, got_frame); break;
	case 1: ret = enc_func(stream_ctx[stream_index].enc_ctx_2, &enc_pkt, filt_frame, got_frame); break;
	case 2: ret = enc_func(stream_ctx[stream_index].enc_ctx_3, &enc_pkt, filt_frame, got_frame); break;
	case 3: ret = enc_func(stream_ctx[stream_index].enc_ctx_4, &enc_pkt, filt_frame, got_frame); break;
	}
	//ret = enc_func(stream_ctx[stream_index].enc_ctx_1, &enc_pkt,filt_frame, got_frame);

	av_frame_free(&filt_frame);
	if (ret < 0)
		return ret;
	if (!(*got_frame))
		return 0;

	/* prepare packet for muxing */
	enc_pkt.stream_index = stream_index;

	switch (encodNum) {
	case 0:av_packet_rescale_ts(&enc_pkt, stream_ctx[stream_index].enc_ctx_1->time_base, ofmt_ctx_1->streams[stream_index]->time_base);
		av_log(NULL, AV_LOG_INFO, "Muxing frame\n");
		ret = av_interleaved_write_frame(ofmt_ctx_1, &enc_pkt); break;
		
	case 1:av_packet_rescale_ts(&enc_pkt, stream_ctx[stream_index].enc_ctx_2->time_base, ofmt_ctx_2->streams[stream_index]->time_base);
		av_log(NULL, AV_LOG_INFO, "Muxing frame\n");
		/* mux encoded frame */
		ret = av_interleaved_write_frame(ofmt_ctx_2, &enc_pkt); break;

	case 2:av_packet_rescale_ts(&enc_pkt, stream_ctx[stream_index].enc_ctx_3->time_base, ofmt_ctx_3->streams[stream_index]->time_base);
		av_log(NULL, AV_LOG_INFO, "Muxing frame\n");
		/* mux encoded frame */
		ret = av_interleaved_write_frame(ofmt_ctx_3, &enc_pkt); break;

	case 3:av_packet_rescale_ts(&enc_pkt, stream_ctx[stream_index].enc_ctx_4->time_base, ofmt_ctx_4->streams[stream_index]->time_base);
		av_log(NULL, AV_LOG_INFO, "Muxing frame\n");
		/* mux encoded frame */
		ret = av_interleaved_write_frame(ofmt_ctx_4, &enc_pkt); break;
	}
	/*av_packet_rescale_ts(&enc_pkt,stream_ctx[stream_index].enc_ctx_1->time_base,ofmt_ctx_1->streams[stream_index]->time_base);
	av_log(NULL, AV_LOG_INFO, "Muxing frame\n");
	/* mux encoded frame */
	/*ret = av_interleaved_write_frame(ofmt_ctx_1, &enc_pkt);*/

	return ret;
}

int Encoder::filter_encode_write_frame(AVFrame* frame, unsigned int stream_index, int encoderNum)
{
	int ret;
	AVFrame* filt_frame;

	av_log(NULL, AV_LOG_INFO, "Pushing decoded frame to filters\n");
	/* push the decoded frame into the filtergraph */
	ret = av_buffersrc_add_frame_flags(filter_ctx[encoderNum][stream_index].buffersrc_ctx,
		frame, 0);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
		return ret;
	}

	/* pull filtered frames from the filtergraph */
	while (1) {
		filt_frame = av_frame_alloc();
		if (!filt_frame) {
			ret = AVERROR(ENOMEM);
			break;
		}
		av_log(NULL, AV_LOG_INFO, "Pulling filtered frame from filters\n");
		ret = av_buffersink_get_frame(filter_ctx[encoderNum][stream_index].buffersink_ctx,
			filt_frame);
		if (ret < 0) {
			/* if no more frames for output - returns AVERROR(EAGAIN)
			 * if flushed and no more frames for output - returns AVERROR_EOF
			 * rewrite retcode to 0 to show it as normal procedure completion
			 */
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				ret = 0;
			av_frame_free(&filt_frame);
			break;
		}

		filt_frame->pict_type = AV_PICTURE_TYPE_NONE;
		ret = encode_write_frame(filt_frame, stream_index, NULL, encoderNum);
		if (ret < 0)
			break;
	}

	return ret;
}

int Encoder::flush_encoder(unsigned int stream_index)
{
	int ret =-1;
	int got_frame;

	if (!(stream_ctx[stream_index].enc_ctx_1->codec->capabilities &
		AV_CODEC_CAP_DELAY))
		return 0;

	while (1) {
		av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder\n", stream_index);

		for (int encodeNum = 0; encodeNum < 4; encodeNum++) {
			ret = encode_write_frame(NULL, stream_index, &got_frame, encodeNum);
			//encode(c, NULL, pkt, f);
		}
	

		if (ret < 0)
			break;
		if (!got_frame)
			return 0;
	}
	return ret;
}

int main(int argc, char** argv)
{
	Encoder encoder;
	int ret;
	//AVPacket packet = { .data = NULL,.size = 0 };
	AVPacket packet;
	packet.data = NULL;
	packet.size = 0;
	AVFrame* frame = NULL;
	enum AVMediaType type;
	unsigned int stream_index;
	unsigned int i;
	int got_frame;
	int(*dec_func)(AVCodecContext*, AVFrame*, int*, const AVPacket*);
	int frameCounter = 0;
	int frameCounterMod;


	/*if (argc != 3) {
		av_log(NULL, AV_LOG_ERROR, "Usage: %s <input file> <output file>\n", argv[0]);
		return 1;
	}
	*/

	if ((ret = encoder.open_input_file("E:\\SelfLearning\\Rubiks\\RubiksVideos\\frame_0_2\\ocean_nosound_720p_0_2.mp4")) < 0)
		goto end;
	if ((ret = encoder.open_output_file("E:\\output2_1.mp4", "E:\\output2_2.mp4", "E:\\output2_3.mp4", "E:\\output2_4.mp4")) < 0)
		goto end;
	for (int i = 0; i < 4; i++) {
		if ((ret = encoder.init_filters(i)) < 0)
			goto end;
	}

	/* read all packets */
	while (1) {
		if ((ret = av_read_frame(Encoder::ifmt_ctx, &packet)) < 0)
			break;
		stream_index = packet.stream_index;
		type = Encoder::ifmt_ctx->streams[packet.stream_index]->codecpar->codec_type;
		av_log(NULL, AV_LOG_DEBUG, "Demuxer gave frame of stream_index %u\n",
			stream_index);

		if (encoder.filter_ctx[0][stream_index].filter_graph && encoder.filter_ctx[1][stream_index].filter_graph
			&& encoder.filter_ctx[2][stream_index].filter_graph && encoder.filter_ctx[3][stream_index].filter_graph) {
			av_log(NULL, AV_LOG_DEBUG, "Going to reencode&filter the frame\n");
			frame = av_frame_alloc();
			if (!frame) {
				ret = AVERROR(ENOMEM);
				break;
			}
			av_packet_rescale_ts(&packet,encoder.ifmt_ctx->streams[stream_index]->time_base,encoder.stream_ctx[stream_index].dec_ctx->time_base);
			dec_func = (type == AVMEDIA_TYPE_VIDEO) ? avcodec_decode_video2 : avcodec_decode_audio4;
			ret = dec_func(encoder.stream_ctx[stream_index].dec_ctx, frame, &got_frame, &packet);
			if (ret < 0) {
				av_frame_free(&frame);
				av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
				break;
			}

			if (got_frame) {
				frame->pts = frame->best_effort_timestamp;
				av_log(NULL, AV_LOG_INFO, "Packet number  %d  %d\n", frame->pts,frameCounter);
				frameCounterMod = frameCounter % 4;
				if (frameCounter == 0) {
					ret = encoder.filter_encode_write_frame(frame, stream_index,0);
					//av_frame_free(&frame);
					if (ret < 0)
						goto end;

					ret = encoder.filter_encode_write_frame(frame, stream_index, 1);
					//av_frame_free(&frame);
					if (ret < 0)
						goto end;
					
					ret = encoder.filter_encode_write_frame(frame, stream_index, 2);
					//av_frame_free(&frame);
					if (ret < 0)
						goto end;

					ret = encoder.filter_encode_write_frame(frame, stream_index, 3);
					av_frame_free(&frame);
					if (ret < 0)
						goto end;

				}
				else {
					switch (frameCounterMod) {
					case 0 : ret = encoder.filter_encode_write_frame(frame, stream_index, 0);
						//ret = encoder.filter_encode_write_frame(frame, stream_index, 3);
						//ret = encoder.filter_encode_write_frame(frame, stream_index, 1);
						//ret = encoder.filter_encode_write_frame(frame, stream_index, 2);
						av_frame_free(&frame);
						if (ret < 0)
							goto end;
						break;
						
					case 1: ret = encoder.filter_encode_write_frame(frame, stream_index, 1);
						//ret = encoder.filter_encode_write_frame(frame, stream_index, 0);
						//ret = encoder.filter_encode_write_frame(frame, stream_index, 3);
						//ret = encoder.filter_encode_write_frame(frame, stream_index, 2);
						av_frame_free(&frame);
						if (ret < 0)
							goto end;
						break;

					case 2:ret = encoder.filter_encode_write_frame(frame, stream_index, 2);
						//ret = encoder.filter_encode_write_frame(frame, stream_index, 0);
						//ret = encoder.filter_encode_write_frame(frame, stream_index, 3);
						//ret = encoder.filter_encode_write_frame(frame, stream_index, 1);
						av_frame_free(&frame);
						if (ret < 0)
							goto end;
						break;

					case 3:ret = encoder.filter_encode_write_frame(frame, stream_index, 3);
						//ret = encoder.filter_encode_write_frame(frame, stream_index, 0);
						//ret = encoder.filter_encode_write_frame(frame, stream_index, 2);
						//ret = encoder.filter_encode_write_frame(frame, stream_index, 1);
						av_frame_free(&frame);
						if (ret < 0)
							goto end;
						break;
					}
				}
				frameCounter++;
			}
			else {
				av_frame_free(&frame);
			}
		}
		else {
			/* remux this frame without reencoding */
			av_packet_rescale_ts(&packet,
				encoder.ifmt_ctx->streams[stream_index]->time_base,
				encoder.ofmt_ctx_1->streams[stream_index]->time_base);

			ret = av_interleaved_write_frame(encoder.ofmt_ctx_1, &packet);
			if (ret < 0)
				goto end;
		}
		av_packet_unref(&packet);
	}

	/* flush filters and encoders */
	for (i = 0; i < encoder.ifmt_ctx->nb_streams; i++) {
		/* flush filter */
		int numEncoders;
		for (numEncoders = 0; numEncoders < 4; numEncoders++) {
			if (!encoder.filter_ctx[numEncoders][i].filter_graph)
				continue;
			ret = encoder.filter_encode_write_frame(NULL, i, numEncoders);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Flushing filter failed\n");
				goto end;
			}

		}
		
		

		/* flush encoder */
		ret = encoder.flush_encoder(i);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Flushing encoder failed\n");
			goto end;
		}

	}

	av_write_trailer(encoder.ofmt_ctx_1);
	av_write_trailer(encoder.ofmt_ctx_2);
	av_write_trailer(encoder.ofmt_ctx_3);
	av_write_trailer(encoder.ofmt_ctx_4);


end:
	av_packet_unref(&packet);
	av_frame_free(&frame);
	for (i = 0; i < encoder.ifmt_ctx->nb_streams; i++) {
		avcodec_free_context(&encoder.stream_ctx[i].dec_ctx);
		if (encoder.ofmt_ctx_1 &&  encoder.ofmt_ctx_1->nb_streams > i &&  encoder.ofmt_ctx_1->streams[i] && encoder.stream_ctx[i].enc_ctx_1)
			avcodec_free_context(&encoder.stream_ctx[i].enc_ctx_1);

		/*adding new 3 encoders*/
		if (encoder.ofmt_ctx_2 && encoder.ofmt_ctx_2->nb_streams > i && encoder.ofmt_ctx_2->streams[i] && encoder.stream_ctx[i].enc_ctx_2)
			avcodec_free_context(&encoder.stream_ctx[i].enc_ctx_2);

		if (encoder.ofmt_ctx_3 && encoder.ofmt_ctx_3->nb_streams > i && encoder.ofmt_ctx_3->streams[i] && encoder.stream_ctx[i].enc_ctx_3)
			avcodec_free_context(&encoder.stream_ctx[i].enc_ctx_3);

		if (encoder.ofmt_ctx_4 && encoder.ofmt_ctx_4->nb_streams > i && encoder.ofmt_ctx_4->streams[i] && encoder.stream_ctx[i].enc_ctx_4)
			avcodec_free_context(&encoder.stream_ctx[i].enc_ctx_4);

		for (int encodNum = 0; encodNum < 4; encodNum++) {
			if (encoder.filter_ctx && encoder.filter_ctx[encodNum][i].filter_graph)
				avfilter_graph_free(&encoder.filter_ctx[encodNum][i].filter_graph);
		}
		
	}
	av_free(encoder.filter_ctx[0]);
	av_free(encoder.filter_ctx[1]);
	av_free(encoder.filter_ctx[2]);
	av_free(encoder.filter_ctx[3]);
	av_free(encoder.stream_ctx);

	avformat_close_input(&encoder.ifmt_ctx);

	if (encoder.ofmt_ctx_1 && !(encoder.ofmt_ctx_1->oformat->flags & AVFMT_NOFILE))
		avio_closep(&encoder.ofmt_ctx_1->pb);
	avformat_free_context(encoder.ofmt_ctx_1);

	if (encoder.ofmt_ctx_2 && !(encoder.ofmt_ctx_2->oformat->flags & AVFMT_NOFILE))
		avio_closep(&encoder.ofmt_ctx_2->pb);
	avformat_free_context(encoder.ofmt_ctx_2);

	if (encoder.ofmt_ctx_3 && !(encoder.ofmt_ctx_3->oformat->flags & AVFMT_NOFILE))
		avio_closep(&encoder.ofmt_ctx_3->pb);
	avformat_free_context(encoder.ofmt_ctx_3);

	if (encoder.ofmt_ctx_4 && !(encoder.ofmt_ctx_4->oformat->flags & AVFMT_NOFILE))
		avio_closep(&encoder.ofmt_ctx_4->pb);
	avformat_free_context(encoder.ofmt_ctx_4);

	//if (ret < 0)
		/*changed*/
		//av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", av_err2str(ret));

		return ret ? 1 : 0;
}
