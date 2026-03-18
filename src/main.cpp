#include <Geode/Geode.hpp>
#include <Geode/modify/AppDelegate.hpp>
#include <filesystem>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <objc/objc.h>
    #include <objc/message.h>
    #include <CoreFoundation/CoreFoundation.h>
#endif

using namespace geode::prelude;

inline bool fileExists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec) && !ec;
}

inline void copyFile(const std::filesystem::path& from, const std::filesystem::path& to) {
    std::error_code ec;
    std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, ec);
}

inline std::filesystem::path makePath(const std::filesystem::path& base, const std::string& file) {
    return base / file;
}

void CopyFromLocal() {
    auto gameFile = makePath(Mod::get()->getConfigDir(), "settings.json");
    auto saveFile = makePath(Mod::get()->getSaveDir(), "settings.json");

    if (!fileExists(gameFile)) return;
    copyFile(gameFile, saveFile);
}

void CopyFromData() {
    auto gameFile = makePath(Mod::get()->getConfigDir(), "settings.json");
    auto saveFile = makePath(Mod::get()->getSaveDir(), "settings.json");

    if (!fileExists(saveFile)) return;
    copyFile(saveFile, gameFile);
}

inline void updateWindowTitle() {
    std::string name = Mod::get()->getSettingValue<std::string>("windowname");

#if defined(_WIN32)
    HWND hwnd = GetActiveWindow();
    if (hwnd) SetWindowTextA(hwnd, name.c_str());
#else
    std::string nameCopy = name;

    dispatch_async(dispatch_get_main_queue(), ^{
        id (*msgSend_id)(id, SEL) = (id (*)(id, SEL))objc_msgSend;
        id (*msgSend_id_id)(id, SEL, id) = (id (*)(id, SEL, id))objc_msgSend;

        id nsApp = msgSend_id((id)objc_getClass("NSApplication"), sel_getUid("sharedApplication"));
        id mainWindow = msgSend_id(nsApp, sel_getUid("mainWindow"));

        if (!mainWindow) return;

        CFStringRef cfStr = CFStringCreateWithCString(
            NULL,
            nameCopy.c_str(),
            kCFStringEncodingUTF8
        );

        if (!cfStr) return;

        msgSend_id_id(mainWindow, sel_getUid("setTitle:"), (id)cfStr);
        CFRelease(cfStr);
    });
#endif
}

#if defined(_WIN32)
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#endif

inline void updateWindowIcon() {
    auto iconPath = Mod::get()->getSettingValue<std::filesystem::path>("windowicon");

#if defined(_WIN32)
    HWND hwnd = GetActiveWindow();
    if (!hwnd || !fileExists(iconPath)) return;

    static ULONG_PTR gdiplusToken = 0;
    if (!gdiplusToken) {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
    }

    Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromFile(iconPath.wstring().c_str(), FALSE);
    if (!bitmap || bitmap->GetLastStatus() != Gdiplus::Ok) {
        delete bitmap;
        return;
    }

    HICON hIcon = nullptr;
    if (bitmap->GetHICON(&hIcon) == Gdiplus::Ok && hIcon) {
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    }

    delete bitmap;

#else
    if (!fileExists(iconPath)) return;

    auto pathU8 = iconPath.u8string();
    std::string pathCopy(reinterpret_cast<const char*>(pathU8.c_str()));

    dispatch_async(dispatch_get_main_queue(), ^{
        id (*msgSend_id)(id, SEL) = (id (*)(id, SEL))objc_msgSend;
        id (*msgSend_id_id)(id, SEL, id) = (id (*)(id, SEL, id))objc_msgSend;
        id (*msgSend_id_cf)(id, SEL, CFStringRef) = (id (*)(id, SEL, CFStringRef))objc_msgSend;

        id nsApp = msgSend_id((id)objc_getClass("NSApplication"), sel_getUid("sharedApplication"));
        if (!nsApp) return;

        CFStringRef pathStr = CFStringCreateWithCString(
            NULL,
            pathCopy.c_str(),
            kCFStringEncodingUTF8
        );

        if (!pathStr) return;

        id nsURLClass = (id)objc_getClass("NSURL");
        id nsURL = msgSend_id_cf(nsURLClass, sel_getUid("fileURLWithPath:"), pathStr);

        if (!nsURL) {
            CFRelease(pathStr);
            return;
        }

        id nsImageClass = (id)objc_getClass("NSImage");
        id image = msgSend_id(nsImageClass, sel_getUid("alloc"));
        image = msgSend_id_id(image, sel_getUid("initWithContentsOfURL:"), nsURL);

        if (image != nullptr) {
            msgSend_id_id(nsApp, sel_getUid("setApplicationIconImage:"), image);
        }

        CFRelease(pathStr);
    });
#endif
}

#include <Geode/modify/LoadingLayer.hpp>
class $modify(LoadingLayer) {
    bool init(bool p0) {
        if (!LoadingLayer::init(p0))
            return false;
        updateWindowTitle();
        updateWindowIcon();
        return true;
    }
};

$on_mod(Loaded) {
    CopyFromLocal();
    (void)Mod::get()->loadData();
    listenForSettingChanges<std::string>("windowname", [](std::string) {
        updateWindowTitle();
    });
    listenForSettingChanges<std::filesystem::path>("windowicon", [](const std::filesystem::path&) {
        updateWindowIcon();
    });
}

$on_mod(DataSaved) {
    CopyFromData();
}
