#include "imgui_ui.h"
#include "settings.h"

#include <time.h>
#include <game_window_manager.h>
#include <mcpelauncher/path_helper.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_opengl3.h>
#include <build_info.h>
#include <GLES3/gl3.h>
#if defined(__i386__) || defined(__x86_64__)
#include "cpuid.h"
#endif
#include <string_view>
#include <log.h>
#include <chrono>

static double g_Time = 0.0;
static bool allowGPU = true;

static std::string_view myGlGetString(GLenum t) {
    auto raw = glGetString(t);
    if(!raw) {
        return {};
    }
    return (const char*)raw;
}

void ImGuiUIInit(GameWindow* window) {
    if(!glGetString) {
        return;
    }
    Log::info("GL", "Vendor: %s\n", glGetString(GL_VENDOR));
    Log::info("GL", "Renderer: %s\n", glGetString(GL_RENDERER));
    Log::info("GL", "Version: %s\n", glGetString(GL_VERSION));
    if(!Settings::enable_imgui.value_or(allowGPU) || ImGui::GetCurrentContext()) {
        return;
    }
    if(!Settings::enable_imgui.has_value() ) {
        allowGPU = GLAD_GL_ES_VERSION_3_0;
        if(!allowGPU) {
            Log::error("ImGuiUIInit", "Disabling ImGui Overlay due to OpenGLES 2");
        }
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    static std::string ininame = PathHelper::getPrimaryDataDirectory() + "imgui.ini";
    io.IniFilename = ininame.data();

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();
    io.BackendPlatformName = "imgui_impl_mcpelauncher";
    io.ClipboardUserData = window;
    io.SetClipboardTextFn = [](void *user_data, const char *text) {
        if(text != nullptr) {
            ((GameWindow *)user_data)->setClipboardText(text);
        }
    };
    io.GetClipboardTextFn = [](void *user_data) -> const char* {
        return Settings::clipboard.data();
    };
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplOpenGL3_Init("#version 100");

    auto modes = window->getFullscreenModes();
    for(auto&& mode : modes) {
        if(Settings::videoMode == mode.description) {
            window->setFullscreenMode(mode);
        }
    }

    // auto && style = ImGui::GetStyle();
    // style.Colors[ImGuiCol_Border]                = ImVec4(0.31f, 0.31f, 1.00f, 0.00f);
    // style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    // style.Colors[ImGuiCol_Button] = ImVec4(0x1e / 255.0, 0x1e / 255.0, 0x1e / 255.0, 0xff);
    // //style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0x1e / 255.0, 0x1e / 255.0, 0x1e / 255.0, 0xff);
    // style.Colors[ImGuiCol_ButtonActive] = ImVec4(0x30 / 255.0, 0x30 / 255.0, 0x30 / 255.0, 0xff);

}

void ImGuiUIDrawFrame(GameWindow* window) {
    if(!Settings::enable_imgui.value_or(allowGPU) || !glViewport) {
        return;
    }
    ImGuiIO& io = ImGui::GetIO();
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();

    // Setup display size (every frame to accommodate for window resizing)
    int32_t window_width;
    int32_t window_height;
    window->getWindowSize(window_width, window_height);
    int display_width = window_width;
    int display_height = window_height;

    io.DisplaySize = ImVec2((float)window_width, (float)window_height);
    if (window_width > 0 && window_height > 0)
        io.DisplayFramebufferScale = ImVec2((float)display_width / window_width, (float)display_height / window_height);

    // Setup time step
    struct timespec current_timespec;
    clock_gettime(CLOCK_MONOTONIC, &current_timespec);
    double current_time = (double)(current_timespec.tv_sec) + (current_timespec.tv_nsec / 1000000000.0);
    io.DeltaTime = g_Time > 0.0 ? (float)(current_time - g_Time) : (float)(1.0f / 60.0f);
    g_Time = current_time;

    ImGui::NewFrame();
    static auto showMenuBar = true;
    static auto menuFocused = false;
    auto now = std::chrono::high_resolution_clock::now();
    static auto mouseOnY0Since = now;
    bool showMenuBarViaMouse = false;
    if(io.MousePos.y) {
        mouseOnY0Since = now;
    } else {
        auto secs = std::chrono::duration_cast<std::chrono::milliseconds>(now - mouseOnY0Since).count();
        showMenuBarViaMouse = secs >= 500;
    }
    auto autoShowMenubar = (!window->getFullscreen() || showMenuBarViaMouse || menuFocused) && !window->getCursorDisabled();
    static auto showFilePicker = false;
    static auto show_demo_window = false;
    static auto show_confirm_popup = false;
    static auto show_about = false;
    auto wantfocusnextframe = io.KeyAlt;
    if(wantfocusnextframe) {
        ImGui::SetNextFrameWantCaptureKeyboard(true);
    }
    static bool lastwantfocusnextframe = false;
    if(Settings::enable_menubar && showMenuBar && (autoShowMenubar || wantfocusnextframe) && ImGui::BeginMainMenuBar())

    {
        menuFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) || ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
        if(wantfocusnextframe) {
            auto w = ImGui::GetCurrentWindow();
            if(!lastwantfocusnextframe) {
                //auto id = w->GetID(0);
                auto id = ImGui::GetID("File");
                ImGui::SetFocusID(id, w);
                //ImGui::SetActiveID(id, w);
                ImGuiContext& g = *ImGui::GetCurrentContext();
                g.NavDisableHighlight = false;
            }
            //w->Active = true;
            menuFocused = true;
        }
        lastwantfocusnextframe = wantfocusnextframe;
        if(ImGui::BeginMenu("File")) {
#ifndef NDEBUG
            if(ImGui::MenuItem("Open")) {
                showFilePicker = true;
            }
#endif
            if(ImGui::MenuItem("Hide Menubar")) {
                show_confirm_popup = true;
            }
#ifndef NDEBUG
            if(ImGui::MenuItem("Show Demo")) {
                show_demo_window = true;
            }
#endif
            if(ImGui::MenuItem("Close")) {
                window->close();
            }
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("Mods")) {
            if(ImGui::MenuItem("Enable Keyboard Tab/Up/Down Patches for 1.20.60+", nullptr, Settings::enable_keyboard_tab_patches_1_20_60)) {
                Settings::enable_keyboard_tab_patches_1_20_60 ^= true;
                Settings::save();
            }
            if(ImGui::MenuItem("Enable Keyboard AutoFocus Patches for 1.20.60+", nullptr, Settings::enable_keyboard_autofocus_patches_1_20_60)) {
                Settings::enable_keyboard_autofocus_patches_1_20_60 ^= true;
                Settings::save();
            }
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("View")) {
            if(ImGui::BeginMenu("Show FPS-Hud")) {
                if (ImGui::MenuItem("None", nullptr, Settings::fps_hud_location == -1)) {
                    Settings::fps_hud_location = -1;
                    Settings::save();
                }
                if (ImGui::MenuItem("Top Left", nullptr, Settings::fps_hud_location == 0)) {
                    Settings::fps_hud_location = 0;
                    Settings::save();
                }
                if (ImGui::MenuItem("Top Right", nullptr, Settings::fps_hud_location == 1)) {
                    Settings::fps_hud_location = 1;
                    Settings::save();
                }
                if (ImGui::MenuItem("Bottom Left", nullptr, Settings::fps_hud_location == 2)) {
                    Settings::fps_hud_location = 2;
                    Settings::save();
                }
                if (ImGui::MenuItem("Bottom Right", nullptr, Settings::fps_hud_location == 3)) {
                    Settings::fps_hud_location = 3;
                    Settings::save();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("Video")) {
            auto modes = window->getFullscreenModes();
            if(ImGui::MenuItem("Toggle Fullscreen", nullptr, window->getFullscreen())) {
                window->setFullscreen(!window->getFullscreen());
            }
            if(!modes.empty()) {
                ImGui::Separator();
            }
            for(auto&& mode : modes) {
                if(ImGui::MenuItem(mode.description.data(), nullptr, mode.id == window->getFullscreenMode().id)) {
                    window->setFullscreenMode(mode);
                    Settings::videoMode = mode.description;
                    Settings::save();
                }
            }
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("Help")) {
            ImGui::MenuItem("About", nullptr, &show_about);
            ImGui::EndMenu();
        }
        auto size = ImGui::GetWindowSize();
        Settings::menubarsize = (int)size.y;
        ImGui::EndMainMenuBar();
    } else {
        Settings::menubarsize = 0;
        menuFocused = false;
        lastwantfocusnextframe = false;
    }
    // Always center this window when appearing
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    auto name = "Hide Menubar until exit?";
    if(show_confirm_popup) {
        show_confirm_popup = false;
        ImGui::OpenPopup(name);
    }
    if (ImGui::BeginPopupModal(name, NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        static bool rememberMyDecision = false;
        if(rememberMyDecision) {
            ImGui::TextWrapped("After doing this you cannot access the functionality provided by the menubar until you manually change/delete the settings file");
        } else {
            ImGui::TextWrapped("After doing this you cannot access the functionality provided by the menubar until you restart Minecraft");
        }
        ImGui::Separator();
        ImGui::Checkbox("Remember my Decision Forever (a really long time)", &rememberMyDecision);
        ImGui::Separator();

        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
            showMenuBar = false;
            if(rememberMyDecision) {
                Settings::enable_menubar = false;
                Settings::save();
            }
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    if(show_about) {
        if(ImGui::Begin("About", &show_about)) {
            ImGui::Text("mcpelauncher-client %s / manifest %s\n", CLIENT_GIT_COMMIT_HASH, MANIFEST_GIT_COMMIT_HASH);
#if defined(__i386__) || defined(__x86_64__)
            CpuId cpuid;
            ImGui::Text("CPU: %s %s\n", cpuid.getManufacturer(), cpuid.getBrandString());
            ImGui::Text("SSSE3 support: %s\n", cpuid.queryFeatureFlag(CpuId::FeatureFlag::SSSE3) ? "YES" : "NO");
#endif
            ImGui::Text("GL Vendor: %s\n", glGetString(0x1F00 /* GL_VENDOR */));
            ImGui::Text("GL Renderer: %s\n", glGetString(0x1F01 /* GL_RENDERER */));
            ImGui::Text("GL Version: %s\n", glGetString(0x1F02 /* GL_VERSION */));

            auto id = ImGui::GetID("Perfectly");
            bool hovered = id == ImGui::GetHoveredID();
            bool active = id == ImGui::GetActiveID();
            ImGui::Text("Hovered: %d\n", hovered);
            auto&& col3 = [](long long c) {
                return ImVec4(((c >> 8) & 0xf) / 16.0, ((c >> 4) & 0xf) / 16.0, ((c) & 0xf) / 16.0, 1);
            };
            auto&& col6 = [](long long c) {
                return ImVec4(((c >> 16) & 0xff) / 255.0, ((c >> 8) & 0xff) / 255.0, ((c) & 0xff) / 255.0, 1);
            };
            ImGui::PushStyleColor(ImGuiCol_NavHighlight, col6(0x00ff00));
            ImGui::PushStyleColor(ImGuiCol_Text, col6(0xffffff));
            ImGui::PushStyleColor(ImGuiCol_Button, col6(0x1e1e1e));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, col3(0x333));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col6(0x1e1e1e));
            ImGui::PushStyleColor(ImGuiCol_Border, active ? col3(0x888) : hovered ? col3(0x666) : col3(0x555));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 5));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2);
            ImGui::Button("Perfectly", ImVec2(0, 40));
            ImGui::Text("Hovered Post: %d\n", ImGui::IsItemHovered());

            ImGui::PopStyleVar(3);
            ImGui::PopStyleColor(6);
        }
        ImGui::End();
    }
    if(showFilePicker) {
        if(ImGui::Begin("filepicker", &showFilePicker)) {
            static char path[256];
            ImGui::InputText("Path", path, 256);
            if(ImGui::Button("Open")) {
                
            }
        }
        ImGui::End();
    }
    if (show_demo_window)
        ImGui::ShowDemoWindow(&show_demo_window);

    if (Settings::fps_hud_location >= 0)
    {
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
        const float PAD = 10.0f;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
        ImVec2 work_size = viewport->WorkSize;
        ImVec2 window_pos, window_pos_pivot;
        window_pos.x = (Settings::fps_hud_location & 1) ? (work_pos.x + work_size.x - PAD) : (work_pos.x + PAD);
        window_pos.y = (Settings::fps_hud_location & 2) ? (work_pos.y + work_size.y - PAD) : (work_pos.y + PAD);
        window_pos_pivot.x = (Settings::fps_hud_location & 1) ? 1.0f : 0.0f;
        window_pos_pivot.y = (Settings::fps_hud_location & 2) ? 1.0f : 0.0f;
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        window_flags |= ImGuiWindowFlags_NoMove;
        ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
        if (ImGui::Begin("fps-hud", nullptr, window_flags))
        {
            ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        }
        ImGui::End();
        
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1, 1, 1, 1));

        ImGui::SetNextWindowPos(ImVec2(work_pos.x + PAD + 30, work_pos.y + PAD), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
        window_flags |= ImGuiWindowFlags_NoMove;
        ImGui::SetNextWindowBgAlpha(ImGui::GetKeyData(ImGuiKey_W)->Down ? 0.70f : 0.35f); // Transparent background
        ImVec2 size;
        if (ImGui::Begin("W", nullptr, window_flags))
        {
            ImGui::Text("W");
            size = ImGui::GetWindowSize();
        }
        ImGui::End();
        
        auto x = work_pos.x + PAD;
        auto y = work_pos.y + PAD + size.y + PAD;
        ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
        window_flags |= ImGuiWindowFlags_NoMove;
        ImGui::SetNextWindowBgAlpha(ImGui::GetKeyData(ImGuiKey_A)->Down ? 0.70f : 0.35f); // Transparent background
        if (ImGui::Begin("A", nullptr, window_flags))
        {
            ImGui::Text("A");
            auto pos = ImGui::GetWindowPos();
            size = ImGui::GetWindowSize();
            x += PAD + size.x;
        }
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
        window_flags |= ImGuiWindowFlags_NoMove;
        ImGui::SetNextWindowBgAlpha(ImGui::GetKeyData(ImGuiKey_S)->Down ? 0.70f : 0.35f); // Transparent background
        if (ImGui::Begin("S", nullptr, window_flags))
        {
            ImGui::Text("S");
            auto pos = ImGui::GetWindowPos();
            auto size = ImGui::GetWindowSize();
            x += PAD + size.x;
        }
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
        window_flags |= ImGuiWindowFlags_NoMove;
        ImGui::SetNextWindowBgAlpha(ImGui::GetKeyData(ImGuiKey_D)->Down ? 0.70f : 0.35f); // Transparent background
        if (ImGui::Begin("D", nullptr, window_flags))
        {
            ImGui::Text("D");
            auto pos = ImGui::GetWindowPos();
            auto size = ImGui::GetWindowSize();
            x += PAD + size.x;
        }
        ImGui::End();

        // ImGui controls
        static int clickCount = 0;
        static int rclickCount = 0;
        clickCount += ImGui::GetMouseClickedCount(ImGuiMouseButton_Left);
        rclickCount += ImGui::GetMouseClickedCount(ImGuiMouseButton_Right);

        static float cps = 0;
        static float rcps = 0;
        ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
        window_flags |= ImGuiWindowFlags_NoMove;
        ImGui::SetNextWindowBgAlpha(ImGui::GetMouseClickedCount(ImGuiMouseButton_Left) > 0 ? 0.70f : 0.35f); // Transparent background

        if (ImGui::Begin("LCPS", nullptr, window_flags))
        {
            ImGui::Text("LCPS: %.2f", cps);
            auto size = ImGui::GetWindowSize();
            x += PAD + size.x;
        }
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
        window_flags |= ImGuiWindowFlags_NoMove;
        ImGui::SetNextWindowBgAlpha(ImGui::GetMouseClickedCount(ImGuiMouseButton_Right) > 0 ? 0.70f : 0.35f); // Transparent background

        if (ImGui::Begin("RCPS", nullptr, window_flags))
        {
            ImGui::Text("RCPS: %.2f", rcps);
            auto size = ImGui::GetWindowSize();
            x += PAD + size.x;
        }
        ImGui::End();



        // Calculate clicks per second
        static float elapsedTime = 0.0f;
        elapsedTime += ImGui::GetIO().DeltaTime;
        if (elapsedTime >= 1.0f) {
            cps = static_cast<float>(clickCount) / elapsedTime;
            rcps = static_cast<float>(rclickCount) / elapsedTime;
            clickCount = 0;
            rclickCount = 0;
            elapsedTime = 0.0f;
        }

        ImGui::PopStyleColor();
    }

    // Rendering
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
