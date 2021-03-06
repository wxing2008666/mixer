#include <algorithm>
#include "mixer_engine.h"

mixer_engine::mixer_engine()
:_eof(true)
,_time_beg(MEDIA_FRAME_NONE_TIMESTAMP)
,_time_end(MEDIA_FRAME_NONE_TIMESTAMP)
,_percent(0)
{
    //ctor
    g_dump.set_class("mixer_engine");
}

mixer_engine::~mixer_engine()
{
    //dtor
}

ret_type mixer_engine::load(const char* template_file)
{
    JCHK(nullptr != template_file,rc_param_invalid)
    try
    {
        stringstream stream(template_file);
        property_tree::ptree pt_soucre;
        property_tree::json_parser::read_json(stream, pt_soucre);
        JCHK(false == pt_soucre.empty(),rc_param_invalid)
        if(!_source)
        {
            _source.reset(new engine_source(this));
            JCHK(_source,rc_new_fail)
            return _source->load(pt_soucre);
        }
    }
    catch (boost::property_tree::json_parser::json_parser_error& e)
    {
        JCHKM(false,rc_param_invalid,e.what())
    }
    catch (boost::property_tree::ptree_bad_path& e)
    {
        JCHKM(false,rc_param_invalid,e.what())
    }
    catch (boost::property_tree::ptree_bad_data& e)
    {
        JCHKM(false,rc_param_invalid,e.what())
    }
    catch(...)
    {
        JCHKM(false,rc_param_invalid,FORMAT_STR("load template fail,file=%1%",%template_file))
    }
    return rc_ok;
}

ret_type mixer_engine::run(const char* task_file)
{
    JCHK(nullptr != task_file,rc_param_invalid)
    JCHK(_source,rc_state_invalid)
    ret_type rt = rc_ok;

    try
    {
        stringstream stream(task_file);
        property_tree::ptree pt_task;
        property_tree::json_parser::read_json(stream, pt_task);
        JCHK(false == pt_task.empty(),rc_param_invalid)

        optional<property_tree::ptree&> pt_outputs = pt_task.get_child_optional("outputs");
        optional<property_tree::ptree&> pt_segments = pt_task.get_child_optional("inputs");
        JCHK(pt_outputs,rc_param_invalid)
        JCHK(pt_segments,rc_param_invalid)
        JCHK(false == pt_outputs.value().empty(),rc_param_invalid)
        JCHK(false == pt_segments.value().empty(),rc_param_invalid)

        BOOST_FOREACH(property_tree::ptree::value_type &pt_segment, pt_segments.value())
        {
            JIF(_source->append(pt_segment.second))
        }

        BOOST_FOREACH(property_tree::ptree::value_type &pt_output, pt_outputs.value())
        {
            optional<string> url = pt_output.second.get_optional<string>("url");
            JCHK(url&&!url.value().empty(),rc_param_invalid)

            optional<property_tree::ptree&> pt_trackers = pt_output.second.get_child_optional("streams");
            JCHK(pt_trackers,rc_param_invalid)

            render_ptr render = create_filter<media_render>(url.value().c_str());
            JCHK(render,rc_param_invalid)
            BOOST_FOREACH(property_tree::ptree::value_type &pt_stream, pt_trackers.value())
            {
                mixer_ptr mixer;
                media_ptr mt;
                input_pin_ptr pin;
                string path = pt_stream.second.get_value<string>();
                JCHKM(mixer = _source->find(path),rc_param_invalid,FORMAT_STR("can not find mixer output,path=%1%",%path))
                if(mixer->_have_segments)
                {
                    JCHK(mt = mixer->find_output(path),rc_param_invalid)
                    JCHK(pin = render->create_pin(mt),rc_param_invalid);
                    JIF(connect(std::dynamic_pointer_cast<output_pin>(mixer),pin));
                }
            }
            optional<property_tree::ptree&> pt_options = pt_output.second.get_child_optional("options");
            JIF(render->open(url.value(),pt_options ? pt_options.value() : property_tree::ptree()))
            _renders.insert(_renders.end(),render);
        }
    }
    catch (boost::property_tree::json_parser::json_parser_error& e)
    {
        JCHKM(false,rc_param_invalid,e.what())
    }
    catch (boost::property_tree::ptree_bad_path& e)
    {
        JCHKM(false,rc_param_invalid,e.what())
    }
    catch (boost::property_tree::ptree_bad_data& e)
    {
        JCHKM(false,rc_param_invalid,e.what())
    }
    catch(...)
    {
        JCHKM(false,rc_param_invalid,FORMAT_STR("load task fail,file=%1%",%task_file))
    }
    _time_beg = _source->get_time_base();
    _time_end = _source->get_time_end();
    _time = MEDIA_FRAME_NONE_TIMESTAMP;
    _eof = false;
    _percent = 0;
    media_task::run();
    return rt;
}

bool mixer_engine::wait(int ms_wait)
{
    if(true == _eof)
        return true;

    _mt_wait.lock();
    if(0 > ms_wait)
    {
        std::unique_lock<std::timed_mutex> lck(_mt_wait);
        return true;
    }
    else
    {
        if(false == _mt_wait.try_lock_for(std::chrono::milliseconds(ms_wait)))
            return false;
        else
        {
            _mt_wait.unlock();
            return true;
        }
    }
}

ret_type mixer_engine::process()
{
    if(_time == MEDIA_FRAME_NONE_TIMESTAMP)
        _time = 0;

    ret_type rt = _source->process(this);
    if(rt == media_task::rc_eof)
    {
        bool eof = true;
        for(RenderIt it = _renders.begin() ; it != _renders.end() ; ++it)
        {
            if(!(*it)->is_eof())
            {
                eof = false;
                break;
            }
        }
        if(eof)
        {
            _renders.clear();
            _eof = true;
            media_task::stop();
            if(!_mt_wait.try_lock())
                _mt_wait.unlock();
        }
        else
            rt = rc_ok;
    }
    else if(rt == rc_ok)
    {
        _time += _source->_duration;
        int64_t length = (_time_end - _time_beg) * 10000;
        if(0 < length)
        {
            int32_t percent = int32_t(_time * 100.0/length);
            if(percent != _percent)
            {
                _percent = percent;
                TRACE(dump::info,FORMAT_STR("process:%1%%%",%_percent))
            }
        }
    }
    return rt;
}
