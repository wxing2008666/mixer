#ifndef FFMPEG_VIDEO_DECODER_H
#define FFMPEG_VIDEO_DECODER_H
#include "global.h"

class ffmpeg_video_decoder : public media_transform
{
    protected:
        AVCodecContext* _ctxCodec;
        AVFrame* _avframe;
        AVPacket _pkt;
    public:
        ffmpeg_video_decoder();
        virtual ~ffmpeg_video_decoder();
        PLUGIN_DECLARE
    protected:
        //media_filter
        ret_type set_media_type(input_pin* pin,media_ptr mt);
        ret_type set_media_type(output_pin* pin,media_ptr mt);
        ret_type process(input_pin* pin,frame_ptr frame);
    private:
        ret_type open(media_type* mt);
        void close();
};

#endif // FFMPEG_VIDEO_DECODER_H
