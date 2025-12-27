#include <Geode/Geode.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/loader/Mod.hpp>
#include <Windows.h>
#include <thread>
#include <atomic>
#include <chrono>

using namespace geode::prelude;

class $modify(MuteOnUnfocusGameManager, GameManager) {
    inline static float s_prevMusicVolume = 1.0f;
    inline static float s_prevSFXVolume = 1.0f;
    inline static bool s_muted = false;
    inline static bool s_lastFocused = true;
    inline static float s_timeSinceLastCheck = 0.0f;

    inline static std::atomic<bool> s_threadRunning{false};
    inline static std::atomic<bool> s_isFocused{true};
    inline static std::thread* s_focusThread = nullptr;
    inline static DWORD s_ourPid = GetCurrentProcessId();

public:
    static void startFocusThread() {
        if (s_threadRunning.load()) {
            return;
        }
        
        s_threadRunning = true;
        log::info("Starting focus detection thread");
        
        s_focusThread = new std::thread([]() {
            int pollInterval = Mod::get()->getSettingValue<int>("poll-interval");
            log::info("Focus thread running with {}ms interval", pollInterval);
            
            while (s_threadRunning.load()) {
                bool isFocused = false;
                if (HWND fg = GetForegroundWindow()) {
                    DWORD pid = 0;
                    GetWindowThreadProcessId(fg, &pid);
                    isFocused = (pid == s_ourPid);
                }
                
                s_isFocused.store(isFocused);
                
                std::this_thread::sleep_for(std::chrono::milliseconds(pollInterval));
            }
            
            log::info("Focus detection thread stopped");
        });
        
        s_focusThread->detach();
    }
    
    static void stopFocusThread() {
        log::info("Stopping focus detection thread");
        s_threadRunning = false;
    }

    void update(float dt) {
        GameManager::update(dt);
        const bool singleThreaded = Mod::get()->getSettingValue<bool>("single-threaded");
        if (!singleThreaded && !s_threadRunning.load()) {
            startFocusThread();
            log::info("Switched to background thread mode");
        } else if (singleThreaded && s_threadRunning.load()) {
            stopFocusThread();
            log::info("Switched to single-threaded mode");
        }

        bool isFocused = false;
        if (singleThreaded) {
            const int pollIntervalMs = Mod::get()->getSettingValue<int>("poll-interval");
            s_timeSinceLastCheck += dt;
            if (s_timeSinceLastCheck < (pollIntervalMs / 1000.0f)) {
                return;
            }
            s_timeSinceLastCheck = 0.0f;

            if (HWND fg = GetForegroundWindow()) {
                DWORD pid = 0;
                GetWindowThreadProcessId(fg, &pid);
                isFocused = (pid == s_ourPid);
            }
        } else {
            isFocused = s_isFocused.load();
        }
        if (isFocused == s_lastFocused) {
            return;
        }
        s_lastFocused = isFocused;

        auto gm = GameManager::sharedState();
        if (!gm) {
            return;
        }

        if (!isFocused) {
            if (!s_muted) {
                s_prevMusicVolume = gm->m_bgVolume;
                s_prevSFXVolume = gm->m_sfxVolume;
                gm->m_bgVolume = 0.0f;
                gm->m_sfxVolume = 0.0f;
                gm->updateMusic();
                s_muted = true;
            }
        } else {
            if (s_muted) {
                gm->m_bgVolume = s_prevMusicVolume;
                gm->m_sfxVolume = s_prevSFXVolume;
                gm->updateMusic();
                s_muted = false;
            }
        }
    }
};

$execute {
    auto mod = Mod::get();
    const bool singleThreaded = mod->getSettingValue<bool>("single-threaded");
    if (singleThreaded) {
        log::info("Linux Mute on Unfocus loaded - single-threaded mode");
    } else {
        log::info("Linux Mute on Unfocus loaded - background thread mode");
        MuteOnUnfocusGameManager::startFocusThread();
    }
}


