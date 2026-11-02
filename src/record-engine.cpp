#include "record-engine.hpp"
#include "canvas-engine.hpp"
#include <obs-frontend-api.h>
#include <util/platform.h>
#include <chrono>
#include <iomanip>
#include <sstream>

RecordEngine &RecordEngine::instance() {
    static RecordEngine inst;
    return inst;
}

RecordEngine::RecordEngine() {}

RecordEngine::~RecordEngine() {
    shutdown();
}

bool RecordEngine::init() {
    return true;
}

void RecordEngine::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (record_output_) {
            obs_output_force_stop(record_output_);
            obs_output_release(record_output_);
            record_output_ = nullptr;
            is_recording_ = false;
        }
        if (replay_output_) {
            obs_output_force_stop(replay_output_);
            obs_output_release(replay_output_);
            replay_output_ = nullptr;
            is_replay_buffer_active_ = false;
        }
    }
    cleanup_encoders();
}

bool RecordEngine::setup_encoders() {
    if (video_encoder_ && audio_encoder_) return true;

    uint32_t w = CanvasEngine::instance().width();
    uint32_t h = CanvasEngine::instance().height();

    // Use hardware encoder (NVENC or fallback to x264)
    obs_data_t *settings = obs_data_create();
    obs_data_set_int(settings, "bitrate", 6000);
    obs_data_set_string(settings, "rate_control", "CBR");
    obs_data_set_string(settings, "preset", "p5"); // NVENC preset

    const char *encoder_id = "jim_nvenc";
    video_encoder_ = obs_video_encoder_create(encoder_id, "JNCanvas_Video_Encoder", settings, nullptr);
    if (!video_encoder_) {
        // Fallback to x264
        obs_data_set_string(settings, "preset", "veryfast");
        video_encoder_ = obs_video_encoder_create("obs_x264", "JNCanvas_Video_Encoder", settings, nullptr);
    }
    obs_data_release(settings);

    if (!video_encoder_) {
        blog(LOG_ERROR, "[JNCanvas] Failed to create video encoder for recording!");
        return false;
    }

    obs_encoder_set_scaled_size(video_encoder_, w, h);

    // Audio Encoder (AAC)
    obs_data_t *audio_settings = obs_data_create();
    obs_data_set_int(audio_settings, "bitrate", 160);
    audio_encoder_ = obs_audio_encoder_create("ffmpeg_aac", "JNCanvas_Audio_Encoder", audio_settings, 0, nullptr);
    obs_data_release(audio_settings);

    return (audio_encoder_ != nullptr);
}

void RecordEngine::cleanup_encoders() {
    if (video_encoder_) {
        obs_encoder_release(video_encoder_);
        video_encoder_ = nullptr;
    }
    if (audio_encoder_) {
        obs_encoder_release(audio_encoder_);
        audio_encoder_ = nullptr;
    }
}

bool RecordEngine::start_recording(const std::string &output_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_recording_) return true;

    if (!setup_encoders()) return false;

    // Generate automatic filename if not provided
    std::string path = output_path;
    if (path.empty()) {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << "JNCanvas_Record_" << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d_%H-%M-%S") << ".mp4";
        path = ss.str();
    }

    obs_data_t *settings = obs_data_create();
    obs_data_set_string(settings, "path", path.c_str());
    obs_data_set_string(settings, "muxer_settings", "movflags=faststart");

    record_output_ = obs_output_create("ffmpeg_muxer", "JNCanvas_Recorder", settings, nullptr);
    obs_data_release(settings);

    if (!record_output_) {
        blog(LOG_ERROR, "[JNCanvas] Failed to create ffmpeg_muxer output!");
        return false;
    }

    obs_output_set_video_encoder(record_output_, video_encoder_);
    obs_output_set_audio_encoder(record_output_, audio_encoder_, 0);

    if (!obs_output_start(record_output_)) {
        blog(LOG_ERROR, "[JNCanvas] Failed to start recording output!");
        obs_output_release(record_output_);
        record_output_ = nullptr;
        return false;
    }

    is_recording_ = true;
    blog(LOG_INFO, "[JNCanvas] Recording started: %s", path.c_str());
    return true;
}

void RecordEngine::stop_recording() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_recording_ || !record_output_) return;

    obs_output_stop(record_output_);
    obs_output_release(record_output_);
    record_output_ = nullptr;
    is_recording_ = false;

    blog(LOG_INFO, "[JNCanvas] Recording stopped.");
}

bool RecordEngine::start_replay_buffer(uint32_t duration_sec) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_replay_buffer_active_) return true;

    if (!setup_encoders()) return false;

    obs_data_t *settings = obs_data_create();
    obs_data_set_int(settings, "max_time_sec", duration_sec > 0 ? duration_sec : 30);
    obs_data_set_int(settings, "max_size_mb", 500); // 500 MB max circular memory guard

    replay_output_ = obs_output_create("replay_buffer", "JNCanvas_ReplayBuffer", settings, nullptr);
    obs_data_release(settings);

    if (!replay_output_) {
        blog(LOG_ERROR, "[JNCanvas] Failed to create replay_buffer output!");
        return false;
    }

    obs_output_set_video_encoder(replay_output_, video_encoder_);
    obs_output_set_audio_encoder(replay_output_, audio_encoder_, 0);

    if (!obs_output_start(replay_output_)) {
        obs_output_release(replay_output_);
        replay_output_ = nullptr;
        return false;
    }

    is_replay_buffer_active_ = true;
    blog(LOG_INFO, "[JNCanvas] Backtrack Replay Buffer started (%u seconds)", duration_sec);
    return true;
}

void RecordEngine::stop_replay_buffer() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_replay_buffer_active_ || !replay_output_) return;

    obs_output_stop(replay_output_);
    obs_output_release(replay_output_);
    replay_output_ = nullptr;
    is_replay_buffer_active_ = false;

    blog(LOG_INFO, "[JNCanvas] Backtrack Replay Buffer stopped.");
}

bool RecordEngine::save_replay_buffer() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_replay_buffer_active_ || !replay_output_) {
        blog(LOG_WARNING, "[JNCanvas] Cannot save replay buffer: buffer is not currently running!");
        return false;
    }

    proc_handler_t *ph = obs_output_get_proc_handler(replay_output_);
    if (!ph) return false;

    calldata_t cd;
    calldata_init(&cd);
    proc_handler_call(ph, "save", &cd);
    calldata_free(&cd);

    blog(LOG_INFO, "[JNCanvas] Backtrack clip saved successfully!");
    return true;
}

