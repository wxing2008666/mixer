// The functions contained in this file are pretty dummy
// and are included only as a placeholder. Nevertheless,
// they *will* get included in the shared library if you
// don't remove them :)
//
// Obviously, you 'll have to write yourself the super-duper
// functions to include in the resulting library...
// Also, it's not necessary to write every function in this file.
// Feel free to add more files in this project. They will be
// included in the resulting library.
#include "global.h"
#include "../inc/media.h"
#include <boost/regex.hpp>
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

using namespace boost::filesystem;

extern "C"
{
    void init(dump* dmp,const char* name)
    {
        g_dump.init(dmp,name);
    }
}

filter_deleter::filter_deleter(const shared_library& lib,PLUGIN_RELEASE_FILTER_FUNC* func_release)
:_lib(lib)
,_func_release(func_release)
{
}

filter_deleter::~filter_deleter()
{
}

void filter_deleter::operator()(media_filter* filter)
{
    if(nullptr != filter)
        _func_release(filter);
}

filter_ptr create_filter(const char* type,const char* info,media_type* mt_input,media_type* mt_output,const string& dir)
{
    if(!exists(dir))
        return filter_ptr();

    uint32_t priority = 0;
    shared_library lib;
    CLS_CREATE_FUNC* cls_create = nullptr;
    PLUGIN_RELEASE_FILTER_FUNC* filter_release = nullptr;

    directory_iterator item_begin(dir);
    directory_iterator item_end;

    for (; item_begin != item_end; ++item_begin)
    {
        path p = *item_begin;
        if(!is_directory(p))
        {
            fs::error_code ec;
            shared_library lib_temp;
            lib_temp.load(p,ec);
            if(!ec &&
                lib_temp.has(PLUGIN_INIT_FUNC_NAME) &&
                lib_temp.has(PLUGIN_ENUM_FILTER_FUNC_NAME) &&
                lib_temp.has(PLUGIN_RELSEAE_FILTER_FUNC_NAME))
            {
                 PLUGIN_INIT_FUNC* init = lib_temp.get<PLUGIN_INIT_FUNC>(PLUGIN_INIT_FUNC_NAME);
                 PLUGIN_ENUM_FILTER_FUNC* enum_filter = lib_temp.get<PLUGIN_ENUM_FILTER_FUNC>(PLUGIN_ENUM_FILTER_FUNC_NAME);
                 PLUGIN_RELEASE_FILTER_FUNC* release_filter = lib_temp.get<PLUGIN_RELEASE_FILTER_FUNC>(PLUGIN_RELSEAE_FILTER_FUNC_NAME);

                init(&g_dump,p.c_str());
                uint32_t index = 0;
                CLS_CREATE_FUNC* create_func = nullptr;
                while(enum_filter(index,type,info,mt_input,mt_output,create_func,priority)){++index;}
                if(create_func != cls_create)
                {
                    lib = lib_temp;
                    cls_create = create_func;
                    filter_release = release_filter;
                }
            }
        }
    }

    if(nullptr != cls_create)
        return filter_ptr(cls_create(),filter_deleter(lib,filter_release));
    else
        return filter_ptr();
}

ret_type connect(output_pin_ptr pin_out,input_pin_ptr pin_in,media_ptr mt_out,media_ptr mt_in,const string& dir)
{
    JCHK(pin_out,rc_param_invalid)
    JCHK(pin_in,rc_param_invalid)

    ret_type rt;
    if(mt_out)
    {
        JIF(pin_out->set_media_type(mt_out))
    }
    else
    {
        JCHK(mt_out = pin_out->get_media_type(),rc_param_invalid)
    }

    if(mt_in)
    {
        JIF(pin_in->set_media_type(mt_in))
    }
    else
    {
        mt_in = pin_in->get_media_type();
        if(!mt_in)
        {
            mt_in = media_type::create();
            JIF(media_type::copy(mt_in,mt_out,false))
            JIF(pin_in->set_media_type(mt_in))
        }
    }

    JIF(media_type::copy(mt_in,mt_out,true))
    if(true == media_type::compare(mt_out,mt_in))
    {
        JIF(pin_out->connect(pin_in,mt_out))
    }
    else
    {
        transform_ptr filter;
        filter = create_filter<media_transform>(nullptr,mt_out,mt_in,dir);
        if(filter)
        {
            JIF(pin_out->connect(filter->get_input_pin()))
            JIF(filter->get_output_pin()->connect(pin_in))
        }
        else
        {
            filter = create_filter<media_transform>(nullptr,mt_out,media_ptr(),dir);
            if(filter)
            {
                JIF(pin_out->connect(filter->get_input_pin()))
                return connect(filter->get_output_pin(),pin_in,media_ptr(),media_ptr(),dir);
            }
            else
            {
                filter = create_filter<media_transform>(nullptr,media_ptr(),mt_in,dir);
                if(filter)
                {
                    JIF(filter->get_output_pin()->connect(pin_in))
                    return connect(pin_out,filter->get_input_pin(),media_ptr(),media_ptr(),dir);
                }
                else
                    return rc_fail;
            }
        }
    }
    return rt;
}

bool str_cmp(const char* str_1,const char* str_2)
{
    if(str_1 == str_2)
        return true;
    if(nullptr == str_1 || nullptr == str_2)
        return false;
    return 0 == strcmp(str_1,str_2);
}

const int VIDEO_ALIGN = 16;
const int AUDIO_ALIGN = 4;

ret_type convert_frame_to_array(media_ptr mt,frame_ptr frame,uint8_t** dst_data,int* dst_linesize)
{
    JCHK(mt,rc_param_invalid)
    JCHK(frame,rc_param_invalid)

    MediaSubType sub = mt->get_sub();
    JCHK(MST_RAWVIDEO == sub || MST_PCM == sub,rc_param_invalid)

    ret_type rt = rc_ok;
    if(MST_RAWVIDEO == sub)
    {
        int szBuf;
        VideoMediaType vmt = mt->get_video_format();
        JCHK(VMT_NONE != vmt,rc_param_invalid)
        int width = mt->get_video_width();
        JCHK(0 < width,rc_param_invalid)
        int height = mt->get_video_height();
        JCHK(0 < height,rc_param_invalid)
        int stride = FFALIGN(width,VIDEO_ALIGN*2);

        JCHK(0 <(szBuf = av_image_get_buffer_size((AVPixelFormat)vmt,stride,height,1)),rc_param_invalid)
        if(0 == frame->get_len())
        {
            JIF(frame->alloc(szBuf))
            frame->_info.stride = stride;
            frame->_info.duration = mt->get_video_duration();
        }
        else
        {
            JCHK(szBuf == (int)frame->get_len(),rc_param_invalid)
        }
        if(nullptr != dst_data && nullptr != dst_linesize)
        {
            JCHK(0 < av_image_fill_arrays(dst_data,dst_linesize,(uint8_t*)frame->get_buf(),(AVPixelFormat)vmt,stride,height,1),rc_fail);
        }
    }
    else if(MST_PCM == sub)
    {
        int szBuf;
        AudioMediaType amt = mt->get_audio_format();
        JCHK(AMT_NONE != amt,rc_param_invalid)
        int channel = mt->get_audio_channel();
        JCHK(0 < channel,rc_param_invalid)
        int frame_size = mt->get_audio_frame_size();
        JCHK(0 < frame_size,rc_param_invalid)
        int sample_rate = mt->get_audio_sample_rate();
        JCHK(0 < sample_rate,rc_param_invalid)

		JCHK(0< (szBuf = av_samples_get_buffer_size(nullptr,channel,frame_size,(AVSampleFormat)amt,AUDIO_ALIGN)),rc_fail);
        if(0 == frame->get_len())
        {
            JIF(frame->alloc(szBuf))
            frame->_info.samples = frame_size;
            frame->_info.duration = mt->get_audio_duration();
        }
        else
        {
            JCHK(szBuf == (int)frame->get_len(),rc_param_invalid)
        }
        if(nullptr != dst_data && nullptr != dst_linesize)
        {
            JCHK(0 < av_samples_fill_arrays(dst_data,dst_linesize,(uint8_t*)frame->get_buf(),channel,frame_size,(AVSampleFormat)amt,AUDIO_ALIGN),rc_fail);
        }
    }
    return rt;
}

int64_t get_local_time()
{
    boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
    boost::posix_time::ptime beg(boost::gregorian::date(1970, boost::gregorian::Jan, 1));
    boost::posix_time::time_duration duration = now - beg;
    return duration.total_microseconds() * 10;
}

bool parse_url(string url,std::vector<std::string>& values)
{
    boost::regex expression("^(\?:([^:/\?#]+)://)\?(\\w+[^/\?#:]*)(\?::(\\d+))\?(/\?(\?:[^\?#/]*/)*)\?([^\?#]*)\?(\\\?(.*))\?");
    return boost::regex_split(std::back_inserter(values), url, expression);
}
