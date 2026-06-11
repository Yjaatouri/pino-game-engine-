#pragma once

#include "engine/core/types.h"
#include "engine/platform/input.h"
#include "engine/core/log.h"
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>

namespace pino {

// -----------------------------------------------------------------------
// Deterministic input recorder for replay / regression testing
// -----------------------------------------------------------------------
// Records the full InputState at each frame boundary.
// Playback restores the state frame-by-frame to reproduce exact behaviour.
// Format: binary file with header + frame-indexed state snapshots.
// -----------------------------------------------------------------------

class InputRecorder {
public:
    enum class Mode : u8 {
        Idle,
        Recording,
        Playback
    };

    InputRecorder() = default;
    ~InputRecorder() { stop(); }

    InputRecorder(const InputRecorder&) = delete;
    InputRecorder& operator=(const InputRecorder&) = delete;

    // ---- Recording ----
    bool start_recording(const char* file_path) {
        stop();
        m_file = std::fopen(file_path, "wb");
        if (!m_file) {
            PINO_ERROR("InputRecorder: failed to open '%s' for writing", file_path);
            return false;
        }
        // Write header
        Header hdr;
        std::memset(&hdr, 0, sizeof(hdr));
        hdr.magic[0] = 'P'; hdr.magic[1] = 'I'; hdr.magic[2] = 'R'; hdr.magic[3] = '1';
        std::fwrite(&hdr, sizeof(hdr), 1, m_file);
        m_frame_count = 0;
        m_mode = Mode::Recording;
        PINO_INFO("InputRecorder: started recording to '%s'", file_path);
        return true;
    }

    void stop_recording() {
        if (m_mode != Mode::Recording) return;
        // Update header with frame count
        if (m_file) {
            std::fseek(m_file, offsetof(Header, frame_count), SEEK_SET);
            std::fwrite(&m_frame_count, sizeof(m_frame_count), 1, m_file);
            std::fclose(m_file);
            m_file = nullptr;
        }
        PINO_INFO("InputRecorder: stopped recording (%u frames)", m_frame_count);
        m_mode = Mode::Idle;
    }

    // ---- Playback ----
    bool start_playback(const char* file_path) {
        stop();
        m_file = std::fopen(file_path, "rb");
        if (!m_file) {
            PINO_ERROR("InputRecorder: failed to open '%s' for reading", file_path);
            return false;
        }
        Header hdr;
        if (std::fread(&hdr, sizeof(hdr), 1, m_file) != 1 ||
            hdr.magic[0] != 'P' || hdr.magic[1] != 'I' ||
            hdr.magic[2] != 'R' || hdr.magic[3] != '1') {
            PINO_ERROR("InputRecorder: invalid recording file '%s'", file_path);
            std::fclose(m_file);
            m_file = nullptr;
            return false;
        }
        m_total_frames = hdr.frame_count;
        m_frame_count = 0;
        m_mode = Mode::Playback;
        PINO_INFO("InputRecorder: started playback from '%s' (%u frames)", file_path, m_total_frames);
        return true;
    }

    void stop_playback() {
        if (m_mode != Mode::Playback) return;
        if (m_file) {
            std::fclose(m_file);
            m_file = nullptr;
        }
        PINO_INFO("InputRecorder: stopped playback (%u/%u frames)", m_frame_count, m_total_frames);
        m_mode = Mode::Idle;
    }

    // ---- Frame hooks ----
    // Called before processing events for a frame.
    // Returns true if there are more frames to replay.
    bool begin_frame(u32 frame_index) {
        m_current_frame = frame_index;
        if (m_mode == Mode::Playback) {
            // Read next frame's state from file
            PlaybackFrame pf;
            if (std::fread(&pf, sizeof(pf), 1, m_file) == 1) {
                auto* in = Input::instance();
                if (in) in->apply_state(pf.state);
                ++m_frame_count;
                return true;
            }
            // End of recording
            stop_playback();
            return false;
        }
        return true;
    }

    // Called after all events for a frame are processed.
    void end_frame() {
        if (m_mode == Mode::Recording) {
            auto* in = Input::instance();
            if (in) {
                PlaybackFrame pf;
                pf.frame_index = m_current_frame;
                in->capture_state(pf.state);
                std::fwrite(&pf, sizeof(pf), 1, m_file);
                ++m_frame_count;
            }
        }
    }

    // ---- State ----
    Mode mode() const { return m_mode; }
    bool is_recording() const { return m_mode == Mode::Recording; }
    bool is_playing() const { return m_mode == Mode::Playback; }
    u32  frame_count() const { return m_frame_count; }
    u32  total_frames() const { return m_total_frames; }

    void stop() {
        if (m_mode == Mode::Recording) stop_recording();
        if (m_mode == Mode::Playback)  stop_playback();
    }

private:
    struct Header {
        char  magic[4];      // "PIR1"
        u32   frame_count = 0;
        u8    reserved[24];
    };

    struct PlaybackFrame {
        u32        frame_index = 0;
        InputState state;
    };

    Mode m_mode = Mode::Idle;
    std::FILE* m_file = nullptr;
    u32 m_current_frame = 0;
    u32 m_frame_count = 0;
    u32 m_total_frames = 0;
};

} // namespace pino
