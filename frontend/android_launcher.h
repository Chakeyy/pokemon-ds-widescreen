#pragma once
#ifdef __ANDROID__

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__has_include)
#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#include <SDL2/SDL_system.h>
#else
#include <SDL.h>
#include <SDL_system.h>
#endif
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_system.h>
#endif
#include <jni.h>

#include "ui.h"

// stb_image implementation for box art pictures (JPEG and PNG)
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "thirdparty/stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION
#include "launcher.h"

namespace AndroidLauncher
{

inline void ShowMessageScreen(const std::string& title, const std::vector<std::string>& lines)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) return;
    SDL_Window* window = SDL_CreateWindow("Pokemon DS Widescreen",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720, SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN);
    if (!window) return;
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) { SDL_DestroyWindow(window); return; }

    SDL_RenderSetLogicalSize(renderer, 1280, 720);

    SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, 1280, 720);
    if (!tex) { SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); return; }

    Canvas canvas;
    canvas.Resize(1280, 720);
    canvas.Fill(0, 0, 1280, 720, 0xFF101018);

    int y = 50;
    auto put = [&](const std::string& str, uint32_t col = 0xFFFFFFFF, int scale = 2) {
        int tw = Canvas::TextWidth(str, scale);
        canvas.Text((1280 - tw) / 2, y, str, scale, col);
        y += 18 * scale;
    };

    put(title, 0xFFFF4444, 3);
    y += 20;
    for (const auto& l : lines)
    {
        if (l.empty()) { y += 14; continue; }
        uint32_t c = 0xFFCCCCCC;
        if (l.find("/sdcard") != std::string::npos || l.find("/storage") != std::string::npos)
            c = 0xFF00E5FF;
        else if (l.find("Pokemon") != std::string::npos || l.find("Tap anywhere") != std::string::npos)
            c = 0xFFFFFFFF;
        put(l, c, 2);
    }

    void* pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(tex, nullptr, &pixels, &pitch) == 0)
    {
        memcpy(pixels, canvas.px.data(), 1280 * 720 * 4);
        SDL_UnlockTexture(tex);
    }
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, tex, nullptr, nullptr);
    SDL_RenderPresent(renderer);

    bool running = true;
    while (running)
    {
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
        {
            if (ev.type == SDL_QUIT || ev.type == SDL_FINGERDOWN ||
                ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_KEYDOWN)
            {
                running = false;
                break;
            }
        }
        SDL_Delay(50);
    }

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}

inline void OpenStorageSettings()
{
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!env || !activity) return;

    jclass activityClass = env->GetObjectClass(activity);
    jclass intentClass = env->FindClass("android/content/Intent");
    jclass uriClass = env->FindClass("android/net/Uri");
    if (!activityClass || !intentClass || !uriClass) return;

    jmethodID getPackageNameMethod = env->GetMethodID(activityClass, "getPackageName", "()Ljava/lang/String;");
    jstring pkgName = (jstring)env->CallObjectMethod(activity, getPackageNameMethod);

    const char* pkgStr = env->GetStringUTFChars(pkgName, nullptr);
    std::string uriStr = std::string("package:") + (pkgStr ? pkgStr : "io.github.clickingmaster.ssrecomp");
    if (pkgStr) env->ReleaseStringUTFChars(pkgName, pkgStr);

    jstring jUriStr = env->NewStringUTF(uriStr.c_str());
    jmethodID parseMethod = env->GetStaticMethodID(uriClass, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
    jobject uriObj = env->CallStaticObjectMethod(uriClass, parseMethod, jUriStr);

    jstring actionManage = env->NewStringUTF("android.settings.MANAGE_APP_ALL_FILES_ACCESS_PERMISSION");
    jmethodID intentInit = env->GetMethodID(intentClass, "<init>", "(Ljava/lang/String;Landroid/net/Uri;)V");
    jobject intentObj = env->NewObject(intentClass, intentInit, actionManage, uriObj);

    jmethodID startActivityMethod = env->GetMethodID(activityClass, "startActivity", "(Landroid/content/Intent;)V");
    env->CallVoidMethod(activity, startActivityMethod, intentObj);

    if (env->ExceptionCheck())
    {
        env->ExceptionClear();
        jstring actionDetails = env->NewStringUTF("android.settings.APPLICATION_DETAILS_SETTINGS");
        jobject intentDetails = env->NewObject(intentClass, intentInit, actionDetails, uriObj);
        env->CallVoidMethod(activity, startActivityMethod, intentDetails);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
}

inline void RequestStoragePermission()
{
    SDL_AndroidRequestPermission("android.permission.READ_EXTERNAL_STORAGE");
}

inline void OpenDocumentsUIFolder()
{
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!env || !activity) return;

    jclass cls = env->GetObjectClass(activity);
    jmethodID mid = env->GetMethodID(cls, "openFolderPicker", "()V");
    if (mid)
    {
        env->CallVoidMethod(activity, mid);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
}

inline void OpenDocumentsUIFile()
{
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!env || !activity) return;

    jclass cls = env->GetObjectClass(activity);
    jmethodID mid = env->GetMethodID(cls, "openFilePicker", "()V");
    if (mid)
    {
        env->CallVoidMethod(activity, mid);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
}

inline std::string CheckDocumentsUIPickedFolder()
{
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!env || !activity) return "";

    jclass cls = env->GetObjectClass(activity);
    jfieldID fid = env->GetStaticFieldID(cls, "sLatestPickedFolder", "Ljava/lang/String;");
    if (!fid) { if (env->ExceptionCheck()) env->ExceptionClear(); return ""; }

    jstring jstr = (jstring)env->GetStaticObjectField(cls, fid);
    if (!jstr) return "";

    const char* chars = env->GetStringUTFChars(jstr, nullptr);
    std::string res = chars ? chars : "";
    if (chars) env->ReleaseStringUTFChars(jstr, chars);

    env->SetStaticObjectField(cls, fid, nullptr);
    return res;
}

inline std::string CheckDocumentsUIPickedRom()
{
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!env || !activity) return "";

    jclass cls = env->GetObjectClass(activity);
    jfieldID fid = env->GetStaticFieldID(cls, "sLatestPickedRom", "Ljava/lang/String;");
    if (!fid) { if (env->ExceptionCheck()) env->ExceptionClear(); return ""; }

    jstring jstr = (jstring)env->GetStaticObjectField(cls, fid);
    if (!jstr) return "";

    const char* chars = env->GetStringUTFChars(jstr, nullptr);
    std::string res = chars ? chars : "";
    if (chars) env->ReleaseStringUTFChars(jstr, chars);

    env->SetStaticObjectField(cls, fid, nullptr);
    return res;
}

inline std::vector<Launcher::Game> ScanAllAndroidGames(const std::string& userDir, const std::string& stateDir)
{
    std::vector<std::string> searchFolders;
    if (!userDir.empty()) searchFolders.push_back(userDir);

    for (const char* dir : {
        "/storage/emulated/0/Download/Roms/NDS",
        "/storage/emulated/0/Download/Roms",
        "/storage/emulated/0/Download/ROMs",
        "/storage/emulated/0/Download",
        "/storage/emulated/0/Roms/NDS",
        "/storage/emulated/0/Roms",
        "/storage/emulated/0/ROMs",
        "/storage/emulated/0/NDS",
        "/storage/emulated/0/RetroArch/downloads",
        "/sdcard/Download",
        "/sdcard/Roms"
    })
    {
        if (std::find(searchFolders.begin(), searchFolders.end(), dir) == searchFolders.end())
        {
            searchFolders.push_back(dir);
        }
    }
    searchFolders.push_back(stateDir + "/roms");

    std::vector<Launcher::Game> games;
    std::vector<std::string> seenRoms;

    for (const auto& folder : searchFolders)
    {
        DIR* d = opendir(folder.c_str());
        if (!d) continue;

        std::vector<std::string> names;
        while (dirent* e = readdir(d))
        {
            if (Launcher::EndsWithNds(e->d_name)) names.push_back(e->d_name);
        }
        closedir(d);
        std::sort(names.begin(), names.end());

        for (const auto& name : names)
        {
            std::string fullPath = folder + (folder.back() == '/' ? "" : "/") + name;
            if (std::find(seenRoms.begin(), seenRoms.end(), fullPath) != seenRoms.end()) continue;

            Launcher::Game g;
            if (Launcher::ReadGame(fullPath, stateDir, g))
            {
                seenRoms.push_back(fullPath);
                games.push_back(std::move(g));
            }
        }
    }

    Launcher::Sort(games);
    return games;
}

inline bool Start(int& argc, char**& argv, const std::string& stateDir)
{
    RequestStoragePermission();

    // Read configured ROM folder
    std::string userRomDir = "/storage/emulated/0/Download";
    std::string romDirFile = stateDir + "/rom_dir.txt";
    if (FILE* f = fopen(romDirFile.c_str(), "r"))
    {
        char buf[1024];
        if (fgets(buf, sizeof buf, f))
        {
            std::string l = buf;
            while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
            if (!l.empty()) userRomDir = l;
        }
        fclose(f);
    }

    // Read last played ROM
    std::string lastRom;
    std::string lastRomFile = stateDir + "/last_rom.txt";
    if (FILE* f = fopen(lastRomFile.c_str(), "r"))
    {
        char buf[1024];
        if (fgets(buf, sizeof buf, f))
        {
            std::string l = buf;
            while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
            if (!l.empty()) lastRom = l;
        }
        fclose(f);
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) < 0) return false;

    SDL_Window* window = SDL_CreateWindow("Pokemon DS Widescreen",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720, SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN);
    if (!window) return false;

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) { SDL_DestroyWindow(window); return false; }

    SDL_RenderSetLogicalSize(renderer, 1280, 720);

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, 1280, 720);
    if (!texture) { SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); return false; }

    Canvas canvas;
    canvas.Resize(1280, 720);

    std::vector<Launcher::Game> games = ScanAllAndroidGames(userRomDir, stateDir);

    int selected = 0;
    int first = 0;
    for (size_t i = 0; i < games.size(); i++)
    {
        if (games[i].rom == lastRom) { selected = (int)i; break; }
    }

    std::string chosenRom = "";
    bool running = true;
    bool showSettings = false;
    Launcher::SettingsPage settingsPage;
    std::string cfgPath = stateDir + "/settings.cfg";

    std::string note = "";
    Uint32 lastFingerTime = 0;

    auto getCanvasCoords = [&](int screenX, int screenY, int& outX, int& outY) {
        int ww = 1, wh = 1;
        SDL_GetWindowSize(window, &ww, &wh);
        outX = screenX * 1280 / ww;
        outY = screenY * 720 / wh;
    };

    auto launchSelected = [&]() {
        if (selected >= 0 && selected < (int)games.size())
        {
            chosenRom = games[selected].rom;
            running = false;
        }
    };

    auto handleLauncherTap = [&](int cx, int cy) {
        if (showSettings)
        {
            // Tap top-right back button
            if (cx >= 1100 && cy <= 60)
            {
                showSettings = false;
                return;
            }
            // Navigate settings item
            if (cy >= 60 && cy < 660)
            {
                int itemIdx = settingsPage.scroll + (cy - 60) / 48;
                if (itemIdx >= 0 && itemIdx < (int)settingsPage.items.size())
                {
                    settingsPage.selected = itemIdx;
                    settingsPage.Change(1);
                }
            }
            return;
        }

        // Bottom action bar (y >= 650)
        if (cy >= 650)
        {
            // [ CHOOSE FOLDER ] (x: 25..245) -> Opens DocumentsUI
            if (cx >= 25 && cx < 245)
            {
                OpenDocumentsUIFolder();
                return;
            }
            // [ OPEN ROM FILE ] (x: 255..475) -> Opens DocumentsUI for single ROM
            if (cx >= 255 && cx < 475)
            {
                OpenDocumentsUIFile();
                return;
            }
            // [ SETTINGS ] (x: 485..630)
            if (cx >= 485 && cx < 630)
            {
                showSettings = !showSettings;
                if (showSettings) settingsPage.Open(cfgPath);
                return;
            }
            // [ PLAY: ... ] (x: 645..1255)
            if (cx >= 645 && cx < 1255 && !games.empty())
            {
                launchSelected();
                return;
            }
            return;
        }

        // Tap on game grid
        if (!games.empty())
        {
            int tile = Launcher::TileAt(canvas, (int)games.size(), first, cx, cy);
            if (tile >= 0)
            {
                if (tile == selected)
                {
                    launchSelected();
                }
                else
                {
                    selected = tile;
                }
            }
        }
        else
        {
            // Empty state buttons:
            // [ CHOOSE ROM FOLDER (DocumentsUI) ]
            if (cy >= 320 && cy < 385 && cx >= 300 && cx < 980)
            {
                OpenDocumentsUIFolder();
            }
            // [ OPEN SPECIFIC .NDS ROM FILE ]
            else if (cy >= 400 && cy < 465 && cx >= 300 && cx < 980)
            {
                OpenDocumentsUIFile();
            }
            // [ GRANT STORAGE ACCESS ]
            else if (cy >= 480 && cy < 545 && cx >= 300 && cx < 980)
            {
                OpenStorageSettings();
            }
        }
    };

    while (running)
    {
        // Check if DocumentsUI picked a folder or ROM in the background
        std::string pickedRom = CheckDocumentsUIPickedRom();
        if (!pickedRom.empty())
        {
            chosenRom = pickedRom;
            running = false;
            break;
        }

        std::string pickedFolder = CheckDocumentsUIPickedFolder();
        if (!pickedFolder.empty() && pickedFolder != userRomDir)
        {
            userRomDir = pickedFolder;
            games = ScanAllAndroidGames(userRomDir, stateDir);
            selected = 0;
            first = 0;
            note = games.empty() ? "NO ROMS FOUND IN FOLDER" : "LOADED " + std::to_string(games.size()) + " GAMES";
        }

        SDL_Event ev;
        while (SDL_PollEvent(&ev))
        {
            if (ev.type == SDL_QUIT)
            {
                running = false;
                break;
            }

            if (ev.type == SDL_KEYDOWN)
            {
                if (ev.key.keysym.sym == SDLK_ESCAPE || ev.key.keysym.sym == SDLK_AC_BACK)
                {
                    if (showSettings) showSettings = false;
                    else { running = false; break; }
                }
                else if (ev.key.keysym.sym == SDLK_LEFT)
                {
                    if (!showSettings && selected > 0) selected--;
                }
                else if (ev.key.keysym.sym == SDLK_RIGHT)
                {
                    if (!showSettings && selected + 1 < (int)games.size()) selected++;
                }
                else if (ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_SPACE)
                {
                    if (!showSettings) launchSelected();
                }
            }

            // Finger touch (normalized 0.0 to 1.0)
            if (ev.type == SDL_FINGERUP)
            {
                lastFingerTime = SDL_GetTicks();
                int cx = (int)(ev.tfinger.x * 1280.0f);
                int cy = (int)(ev.tfinger.y * 720.0f);
                handleLauncherTap(cx, cy);
            }

            // Mouse click
            if (ev.type == SDL_MOUSEBUTTONUP && ev.button.button == SDL_BUTTON_LEFT)
            {
                if (SDL_GetTicks() - lastFingerTime > 150)
                {
                    int cx, cy;
                    getCanvasCoords(ev.button.x, ev.button.y, cx, cy);
                    handleLauncherTap(cx, cy);
                }
            }

            // Mouse wheel for scrolling
            if (ev.type == SDL_MOUSEWHEEL)
            {
                if (showSettings) settingsPage.Move(ev.wheel.y > 0 ? -1 : 1);
                else
                {
                    if (ev.wheel.y > 0 && selected >= 3) selected -= 3;
                    else if (ev.wheel.y < 0 && selected + 3 < (int)games.size()) selected += 3;
                }
            }
        }

        if (!running) break;

        canvas.Fill(0, 0, 1280, 720, 0xFF0B0B0C);

        if (showSettings)
        {
            // Settings Screen
            canvas.Fill(0, 0, 1280, 60, 0xFF141416);
            canvas.Fill(0, 60, 1280, 2, 0xFFE0242C);
            canvas.Text(40, 16, "SETTINGS", 3, 0xFFFFFFFF);
            canvas.Fill(1100, 10, 140, 40, 0xFF35394C);
            canvas.Text(1140, 22, "BACK", 1, 0xFFFFFFFF);

            int y = 80;
            const int s = 2;
            for (int i = settingsPage.scroll; i < (int)settingsPage.items.size() && y < 640; i++)
            {
                const auto& item = settingsPage.items[i];
                const bool sel = i == settingsPage.selected;
                canvas.Fill(40, y - 4, 1200, 44, sel ? 0xFF222638 : 0xFF141620);
                canvas.Text(60, y + 6, item.name, s, sel ? 0xFFFFFFFF : 0xFFB8B8BC);

                if (item.key)
                {
                    std::string val = settingsPage.Shown(item);
                    canvas.Text(800, y + 6, val, s, sel ? 0xFF55FF88 : 0xFFAAAAAA);
                }
                y += 48;
            }
        }
        else if (games.empty())
        {
            // Empty state: No ROMs found
            canvas.Fill(0, 0, 1280, 65, 0xFF141416);
            canvas.Fill(0, 65, 1280, 2, 0xFFE0242C);
            canvas.Text(40, 18, "POKEMON DS WIDESCREEN", 3, 0xFFFFFFFF);

            canvas.Fill(220, 120, 840, 500, 0xFF141620);
            canvas.Fill(218, 118, 844, 2, 0xFFE0242C);

            canvas.Text(360, 160, "NO POKEMON NDS ROMS FOUND", 2, 0xFFFFFFFF);
            canvas.Text(270, 210, "Use Android DocumentsUI below to choose your ROM folder", 1, 0xFFAAAAAA);
            canvas.Text(270, 235, "or pick a specific .nds ROM file directly.", 1, 0xFFAAAAAA);

            // Button 1: Choose Folder
            canvas.Fill(300, 320, 680, 55, 0xFF1E6B38);
            canvas.Text(380, 338, "CHOOSE ROM FOLDER (DocumentsUI)", 2, 0xFFFFFFFF);

            // Button 2: Choose File
            canvas.Fill(300, 400, 680, 55, 0xFF1C4268);
            canvas.Text(390, 418, "OPEN SPECIFIC .NDS ROM FILE", 2, 0xFFFFFFFF);

            // Button 3: Grant Storage Access
            canvas.Fill(300, 480, 680, 55, 0xFF2B3D66);
            canvas.Text(430, 498, "GRANT STORAGE ACCESS PERMISSION", 1, 0xFF88CCFF);
        }
        else
        {
            // Game Grid using launcher.h
            std::string subTitle = "DIR: " + userRomDir + "  (" + std::to_string(games.size()) + " GAMES)";
            if (Canvas::TextWidth(subTitle, 1) > 900) subTitle = subTitle.substr(0, 65) + "...";

            Launcher::Draw(canvas, games, selected, first, note, subTitle);

            // Bottom Action Bar
            canvas.Fill(0, 650, 1280, 70, 0xFF141416);
            canvas.Fill(0, 649, 1280, 2, 0xFF2A2D3C);

            // [ CHOOSE FOLDER ]
            canvas.Fill(25, 660, 220, 48, 0xFF1C4268);
            canvas.Text(55, 676, "CHOOSE FOLDER", 1, 0xFFFFFFFF);

            // [ OPEN ROM FILE ]
            canvas.Fill(255, 660, 220, 48, 0xFF1E4E58);
            canvas.Text(285, 676, "OPEN ROM FILE", 1, 0xFFFFFFFF);

            // [ SETTINGS ]
            canvas.Fill(485, 660, 145, 48, 0xFF2A2E3D);
            canvas.Text(515, 676, "SETTINGS", 1, 0xFFFFFFFF);

            // [ PLAY: GAME NAME ]
            std::string playLabel = "PLAY";
            if (selected >= 0 && selected < (int)games.size())
            {
                playLabel = "PLAY: " + games[selected].name;
            }
            if (Canvas::TextWidth(playLabel, 2) > 560)
            {
                while (!playLabel.empty() && Canvas::TextWidth(playLabel + "...", 2) > 560)
                    playLabel.pop_back();
                playLabel += "...";
            }
            canvas.Fill(645, 660, 610, 48, 0xFF1E6B38);
            canvas.Text(665, 674, playLabel, 2, 0xFFFFFFFF);
        }

        SDL_UpdateTexture(texture, nullptr, canvas.px.data(), 1280 * 4);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        SDL_Delay(16);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    if (chosenRom.empty()) return false;

    // Detect game code
    char code[5] = {};
    if (FILE* f = fopen(chosenRom.c_str(), "rb"))
    {
        if (fseek(f, 0x0C, SEEK_SET) == 0) (void)fread(code, 1, 4, f);
        fclose(f);
    }
    std::string foundCode = code;

    // Save chosen ROM as last played
    if (FILE* f = fopen(lastRomFile.c_str(), "w"))
    {
        fprintf(f, "%s\n", chosenRom.c_str());
        fclose(f);
    }

    // Determine save file name
    std::string saveName = "game.sav";
    if (const Launcher::KnownGame* k = Launcher::Known(foundCode))
    {
        saveName = k->save;
    }
    else if (!foundCode.empty())
    {
        saveName = foundCode + ".sav";
    }
    std::string savePath = stateDir + "/" + saveName;

    static std::vector<std::string> s_args;
    static std::vector<char*> s_argv;

    s_args = { argc > 0 && argv[0] ? argv[0] : "pokemon-ds-wide", chosenRom,
               "--save", savePath, "--fullscreen", "--jit" };

    if (!foundCode.empty() && foundCode != "IPGE" && foundCode != "IPKE")
    {
        s_args.push_back("--funcs");
        s_args.push_back("data/funcs/" + foundCode + ".txt");
    }

    for (auto& a : s_args) s_argv.push_back(a.data());
    s_argv.push_back(nullptr);
    argc = (int)s_args.size();
    argv = s_argv.data();

    return true;
}

} // namespace AndroidLauncher

using AndroidLauncher::ShowMessageScreen;

#endif // __ANDROID__
