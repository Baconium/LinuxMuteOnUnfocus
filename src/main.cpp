#include <Geode/Geode.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Windows.h>

using namespace geode::prelude;

// This mod mutes all game audio when the GD window is not the
// foreground window (e.g. when you alt-tab), and restores the
// previous music & SFX volumes when focus is regained.

class $modify(MuteOnUnfocusGameManager, GameManager) {
    inline static float s_prevMusicVolume = 1.0f;
    inline static float s_prevSFXVolume = 1.0f;
    inline static bool s_muted = false;
    inline static bool s_lastFocused = true;

public:
    void update(float dt) {
        // Call original update first
        GameManager::update(dt);

        // Determine if this process's window is currently focused
        bool isFocused = false;
        if (HWND fg = GetForegroundWindow()) {
            DWORD pid = 0;
            GetWindowThreadProcessId(fg, &pid);
            isFocused = (pid == GetCurrentProcessId());
        }

        // Only do work when focus state changes
        if (isFocused == s_lastFocused) {
            return;
        }
        s_lastFocused = isFocused;

        auto gm = GameManager::sharedState();
        if (!gm) {
            return;
        }

        if (!isFocused) {
            // Lost focus: save volumes and mute
            if (!s_muted) {
                s_prevMusicVolume = gm->m_bgVolume;
                s_prevSFXVolume = gm->m_sfxVolume;
                gm->m_bgVolume = 0.0f;
                gm->m_sfxVolume = 0.0f;
                gm->updateMusic();
                s_muted = true;
            }
        } else {
            // Regained focus: restore volumes
            if (s_muted) {
                gm->m_bgVolume = s_prevMusicVolume;
                gm->m_sfxVolume = s_prevSFXVolume;
                gm->updateMusic();
                s_muted = false;
            }
        }
    }
};

