#pragma once

#include "main_activity.h"
#include "store.h"
#include "../fake_assetmanager.h"
#include <baron/baron.h>
#include <android/native_activity.h>
#include <game_window.h>
#include <condition_variable>
#include <mutex>
#include "../text_input_handler.h"

struct JniSupport {

private:
    struct NativeEntry {
        const char *name;
        const char *sig;
    };

    ANativeActivityCallbacks nativeActivityCallbacks;
    ANativeActivity nativeActivity;
    jobject activityRef;
    std::unique_ptr<FakeAssetManager> assetManager;
    ANativeWindow *window;
    AInputQueue *inputQueue;
    std::condition_variable gameExitCond;
    std::mutex gameExitMutex;
    bool gameExitVal = false, looperRunning = false;

    void registerJniClasses();

    void registerNatives(std::shared_ptr<FakeJni::JClass const> clazz, std::vector<NativeEntry> entries,
                         void *(*symResolver)(const char *));

public:
    Baron::Jvm vm;
    std::shared_ptr<MainActivity> activity;
    TextInputHandler textInput;
    JniSupport();

    void registerMinecraftNatives(void *(*symResolver)(const char *));

    void startGame(ANativeActivity_createFunc *activityOnCreate);

    void stopGame();

    void waitForGameExit();

    void requestExitGame();

    void setLooperRunning(bool running);

    void onWindowCreated(ANativeWindow *window, AInputQueue *inputQueue);

    void onWindowClosed();

    void onWindowResized(int newWidth, int newHeight);

    void onSetTextboxText(std::string const &text);

    void onReturnKeyPressed();

    void setGameControllerConnected(int devId, bool connected);

    TextInputHandler &getTextInputHandler() { return textInput; }

};