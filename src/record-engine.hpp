#pragma once

#include <obs.h>
#include <string>
#include <atomic>
#include <mutex>

class RecordEngine {
public:
    static RecordEngine &instance();

    bool init();
    void shutdown();

    // Recording Controls
    bool start_recording(const std::string &output_path = "");
    void stop_recording();
    bool is_recording() const { return is_recording_; }

    // Replay Buffer (Backtrack) Controls
    bool start_replay_buffer(uint32_t duration_sec = 30);
    void stop_replay_buffer();
    bool save_replay_buffer();
    bool is_replay_buffer_active() const { return is_replay_buffer_active_; }

private:
    RecordEngine();
    ~RecordEngine();

    RecordEngine(const RecordEngine &) = delete;
    RecordEngine &operator=(const RecordEngine &) = delete;

    std::mutex mutex_;
    obs_output_t *record_output_ = nullptr;
    obs_output_t *replay_output_ = nullptr;
    obs_encoder_t *video_encoder_ = nullptr;
    obs_encoder_t *audio_encoder_ = nullptr;

    std::atomic<bool> is_recording_{false};
    std::atomic<bool> is_replay_buffer_active_{false};

    bool setup_encoders();
    void cleanup_encoders();
};

