#ifndef FFMPEG_RENDER_H
#define FFMPEG_RENDER_H
#include "global.h"

class ffmpeg_render : public media_render
{
    protected:
        class stream : public input_pin
        {
            friend class ffmpeg_render;
            protected:
                ffmpeg_render* _render;
                AVStream* _avstream;
                AVBitStreamFilterContext* _ctxBSF;
                AVPacket _pkt_out;
            public:
                stream(ffmpeg_render* render);
                virtual ~stream();
                ret_type deliver(frame_ptr frame);
                ret_type open();
                ret_type convert(AVPacket& pkt,frame_ptr frame);
                void close();
        };
        friend class stream;
        typedef list<std::shared_ptr<stream>> StreamSet;
        typedef StreamSet::iterator StreamIt;
    protected:
        StreamSet _streams;
        AVFormatContext* _ctxFormat;
        stream* _master;
        property_tree::ptree _options;
        string _url;
        bool _is_global_header;
        bool _is_image;
        bool _is_header;
        bool _is_open;
        bool _is_eof;
        AVPacket _pkt_out;
        int64_t _time;
    public:
        ffmpeg_render();
        virtual ~ffmpeg_render();
        PLUGIN_DECLARE
    protected:
        ret_type set_media_type(input_pin* pin,media_ptr mt);
        input_pin_ptr create_pin(media_ptr mt);
        ret_type open(const string& url,const property_tree::ptree& pt);
        bool is_eof();
        bool is_open();
        void close();
        int64_t get_time();
        ret_type process();
        ret_type write(stream* strm,frame_ptr frame);
        ret_type open();
};

#endif // FFMPEG_RENDER_H
