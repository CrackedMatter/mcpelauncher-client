#include <log.h>
#include <dlfcn.h>
#include <game_window_manager.h>
#include <argparser.h>
#include <mcpelauncher/minecraft_utils.h>
#include <mcpelauncher/minecraft_version.h>
#include <mcpelauncher/crash_handler.h>
#include <mcpelauncher/path_helper.h>
#include <mcpelauncher/mod_loader.h>
#include "window_callbacks.h"
#include "splitscreen_patch.h"
#include "gl_core_patch.h"
#include "xbox_live_helper.h"
#include "shader_error_patch.h"
#include "hbui_patch.h"
#include "jni/jni_support.h"
#ifdef USE_ARMHF_SUPPORT
#include "armhf_support.h"
#endif
#if defined(__i386__) || defined(__x86_64__)
#include "cpuid.h"
#include "texel_aa_patch.h"
#include "xbox_shutdown_patch.h"
#endif
#include <build_info.h>
#include <mcpelauncher/patch_utils.h>
#include <libc_shim.h>
#include <mcpelauncher/linker.h>
#include <minecraft/imported/android_symbols.h>
#include "main.h"
#include "fake_looper.h"
#include "fake_assetmanager.h"
#include "fake_egl.h"
#include "symbols.h"
#include "core_patches.h"
#include "thread_mover.h"

static size_t base;
LauncherOptions options;

void printVersionInfo();

int main(int argc, char *argv[]) {
    auto windowManager = GameWindowManager::getManager();
    CrashHandler::registerCrashHandler();
    MinecraftUtils::workaroundLocaleBug();

    argparser::arg_parser p;
    argparser::arg<bool> printVersion (p, "--version", "-v", "Prints version info");
    argparser::arg<std::string> gameDir (p, "--game-dir", "-dg", "Directory with the game and assets");
    argparser::arg<std::string> dataDir (p, "--data-dir", "-dd", "Directory to use for the data");
    argparser::arg<std::string> cacheDir (p, "--cache-dir", "-dc", "Directory to use for cache");
    argparser::arg<int> windowWidth (p, "--width", "-ww", "Window width", 720);
    argparser::arg<int> windowHeight (p, "--height", "-wh", "Window height", 480);
    argparser::arg<bool> disableFmod (p, "--disable-fmod", "-df", "Disables usage of the FMod audio library");
    if (!p.parse(argc, (const char**) argv))
        return 1;
    if (printVersion) {
        printVersionInfo();
        return 0;
    }
    options.windowWidth = windowWidth;
    options.windowHeight = windowHeight;
    options.graphicsApi = GLCorePatch::mustUseDesktopGL() ? GraphicsApi::OPENGL : GraphicsApi::OPENGL_ES2;

    if (!gameDir.get().empty())
        PathHelper::setGameDir(gameDir);
    if (!dataDir.get().empty())
        PathHelper::setDataDir(dataDir);
    if (!cacheDir.get().empty())
        PathHelper::setCacheDir(cacheDir);

    Log::info("Launcher", "Version: client %s / manifest %s", CLIENT_GIT_COMMIT_HASH, MANIFEST_GIT_COMMIT_HASH);
#if defined(__i386__) || defined(__x86_64__)
    {
        CpuId cpuid;
        Log::info("Launcher", "CPU: %s %s", cpuid.getManufacturer(), cpuid.getBrandString());
        Log::info("Launcher", "CPU supports SSSE3: %s",
                cpuid.queryFeatureFlag(CpuId::FeatureFlag::SSSE3) ? "YES" : "NO");
    }
#endif

    Log::trace("Launcher", "Loading hybris libraries");
    linker::init();
    auto libC = MinecraftUtils::getLibCSymbols();
    ThreadMover::hookLibC(libC);
    linker::load_library("libc.so", libC);
    MinecraftUtils::loadLibM();
    MinecraftUtils::setupHybris();
    if (!disableFmod)
        MinecraftUtils::loadFMod();
    FakeEGL::setProcAddrFunction((void *(*)(const char*)) windowManager->getProcAddrFunc());
    FakeEGL::installLibrary();
    MinecraftUtils::setupGLES2Symbols(fake_egl::eglGetProcAddress);

    std::unordered_map<std::string, void*> android_syms;
    FakeAssetManager::initHybrisHooks(android_syms);
    FakeInputQueue::initHybrisHooks(android_syms);
    FakeLooper::initHybrisHooks(android_syms);
    for (auto s = android_symbols; *s; s++) // stub missing symbols
        android_syms.insert({*s, (void *) +[]() { Log::warn("Main", "Android stub called"); }});
    linker::load_library("libandroid.so", android_syms);

#ifdef USE_ARMHF_SUPPORT
    ArmhfSupport::install();
#endif
    linker::update_LD_LIBRARY_PATH(PathHelper::findGameFile(std::string("lib/") + MinecraftUtils::getLibraryAbi()).data());

    Log::trace("Launcher", "Loading Minecraft library");
    static void* handle = MinecraftUtils::loadMinecraftLib();
    Log::info("Launcher", "Loaded Minecraft library");
    Log::debug("Launcher", "Minecraft is at offset 0x%p", (void *) MinecraftUtils::getLibraryBase(handle));
    base = MinecraftUtils::getLibraryBase(handle);

    ModLoader modLoader;
    modLoader.loadModsFromDirectory(PathHelper::getPrimaryDataDirectory() + "mods/");

    Log::info("Launcher", "Game version: %s", MinecraftVersion::getString().c_str());

    Log::info("Launcher", "Applying patches");
    SymbolsHelper::initSymbols(handle);
    CorePatches::install(handle);
#ifdef __i386__
//    XboxShutdownPatch::install(handle);
    TexelAAPatch::install(handle);
    HbuiPatch::install(handle);
    SplitscreenPatch::install(handle);
    ShaderErrorPatch::install(handle);
#endif
    if (options.graphicsApi == GraphicsApi::OPENGL)
        GLCorePatch::install(handle);

    Log::info("Launcher", "Initializing JNI");
    JniSupport support;
    FakeLooper::setJniSupport(&support);
    support.registerMinecraftNatives(+[](const char *sym) {
        return linker::dlsym(handle, sym);
    });
    std::thread startThread([&support]() {
        support.startGame((ANativeActivity_createFunc *) linker::dlsym(handle, "ANativeActivity_onCreate"));
        linker::dlclose(handle);
    });
    startThread.detach();

    Log::info("Launcher", "Executing main thread");
    ThreadMover::executeMainThread();
    support.setLooperRunning(false);

//    XboxLivePatches::workaroundShutdownFreeze(handle);
#ifdef __i386__
    XboxShutdownPatch::notifyShutdown();
#endif

    XboxLiveHelper::getInstance().shutdown();
    return 0;
}

void printVersionInfo() {
    printf("mcpelauncher-client %s / manifest %s\n", CLIENT_GIT_COMMIT_HASH, MANIFEST_GIT_COMMIT_HASH);
#if defined(__i386__) || defined(__x86_64__)
    CpuId cpuid;
    printf("CPU: %s %s\n", cpuid.getManufacturer(), cpuid.getBrandString());
    printf("SSSE3 support: %s\n", cpuid.queryFeatureFlag(CpuId::FeatureFlag::SSSE3) ? "YES" : "NO");
#endif
    auto windowManager = GameWindowManager::getManager();
    GraphicsApi graphicsApi = GLCorePatch::mustUseDesktopGL() ? GraphicsApi::OPENGL : GraphicsApi::OPENGL_ES2;
    auto window = windowManager->createWindow("mcpelauncher", 32, 32, graphicsApi);
    auto glGetString = (const char* (*)(int)) windowManager->getProcAddrFunc()("glGetString");
    printf("GL Vendor: %s\n", glGetString(0x1F00 /* GL_VENDOR */));
    printf("GL Renderer: %s\n", glGetString(0x1F01 /* GL_RENDERER */));
    printf("GL Version: %s\n", glGetString(0x1F02 /* GL_VERSION */));
    printf("MSA daemon path: %s\n", XboxLiveHelper::findMsa().c_str());
}