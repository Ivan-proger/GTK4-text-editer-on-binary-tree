#include <gtkmm.h>
#include <glib.h>
#include <unistd.h>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <iostream>
#include "EditorWindow.h"

// ----------------------- Theme control (ThemeGuard) -----------------------

enum class ThemeMode {
    LOCAL,
    ADWAITA,
    IGNORE_USER
};

class ThemeGuard {
public:
    explicit ThemeGuard(ThemeMode mode) : mode_(mode) {
        const char* curXdg = g_getenv("XDG_CONFIG_HOME");
        oldXdg_ = curXdg ? std::string(curXdg) : std::string();

        const char* curTheme = g_getenv("GTK_THEME");
        oldTheme_ = curTheme ? std::string(curTheme) : std::string();

        if (mode_ == ThemeMode::IGNORE_USER) {
            std::string tmpl = "/tmp/bt_editor_cfgXXXXXX";
            std::vector<char> tmpl_vec(tmpl.begin(), tmpl.end());
            tmpl_vec.push_back('\0'); // mkdtemp требует нуль-терминированную область

            if (mkdtemp(tmpl_vec.data())) { // избегаем отдельной переменной 'created'
                tmpdir_ = std::string(tmpl_vec.data());
                g_setenv("XDG_CONFIG_HOME", tmpdir_.c_str(), TRUE);
            }
        } else if (mode_ == ThemeMode::ADWAITA) {
            g_setenv("GTK_THEME", "Adwaita", TRUE);
        }
    }

    ~ThemeGuard() {
        if (!oldXdg_.empty()) g_setenv("XDG_CONFIG_HOME", oldXdg_.c_str(), TRUE);
        else g_unsetenv("XDG_CONFIG_HOME");

        if (!oldTheme_.empty()) g_setenv("GTK_THEME", oldTheme_.c_str(), TRUE);
        else g_unsetenv("GTK_THEME");

        if (!tmpdir_.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(tmpdir_, ec);
        }
    }

    // запрещаем копирование
    ThemeGuard(const ThemeGuard&) = delete;
    ThemeGuard& operator=(const ThemeGuard&) = delete;

    // кастомные move-операции — нужно корректно передать владение ресурсами
    ThemeGuard(ThemeGuard&& other) noexcept
        : mode_(other.mode_),
          oldXdg_(std::move(other.oldXdg_)),
          oldTheme_(std::move(other.oldTheme_)),
          tmpdir_(std::move(other.tmpdir_))
    {
        // очистим у источника, чтобы его деструктор ничего не сделал
        other.oldXdg_.clear();
        other.oldTheme_.clear();
        other.tmpdir_.clear();
        other.mode_ = ThemeMode::LOCAL;
    }

    ThemeGuard& operator=(ThemeGuard&& other) noexcept {
        if (this == &other) return *this;

        // сначала корректно "отдать" текущие ресурсы (как будто деструктор)
        if (!oldXdg_.empty()) g_setenv("XDG_CONFIG_HOME", oldXdg_.c_str(), TRUE);
        else g_unsetenv("XDG_CONFIG_HOME");

        if (!oldTheme_.empty()) g_setenv("GTK_THEME", oldTheme_.c_str(), TRUE);
        else g_unsetenv("GTK_THEME");

        if (!tmpdir_.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(tmpdir_, ec);
            tmpdir_.clear();
        }

        // теперь принять ресурсы от other
        mode_ = other.mode_;
        oldXdg_ = std::move(other.oldXdg_);
        oldTheme_ = std::move(other.oldTheme_);
        tmpdir_ = std::move(other.tmpdir_);

        // очистить other, чтобы не произошло двойного удаления/восстановления
        other.oldXdg_.clear();
        other.oldTheme_.clear();
        other.tmpdir_.clear();
        other.mode_ = ThemeMode::LOCAL;

        return *this;
    }

private:
    ThemeMode mode_;
    std::string oldXdg_;
    std::string oldTheme_;
    std::string tmpdir_;
};

int main(int argc, char* argv[]) {

    ThemeMode theme_mode = ThemeMode::LOCAL;
    bool inspector_flag = false;
    bool no_wayland_flag = false;

    std::vector<char*> application_argv;
    application_argv.push_back(argv[0]);

    std::vector<std::string_view> args;
    args.reserve(std::max(0, argc - 1));
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);

    for (std::size_t i = 0; i < args.size(); ) {
        std::string_view arg = args[i];

        if (arg == "--inspector") {
            inspector_flag = true;
            ++i;
            continue;
        }

        if (arg == "--no-wayland") {
            no_wayland_flag = true;
            ++i;
            continue;
        }

        // Используем init-statement (C++17) для объявления mode_str прямо в if
        if ((i + 1) < args.size() && (arg == "--theme")) {
            if (auto mode_str = args[i + 1]; mode_str == "adwaita") {
                theme_mode = ThemeMode::ADWAITA;
            } else if (mode_str == "ignore") {
                theme_mode = ThemeMode::IGNORE_USER;
            }
            i += 2;
            continue;
        }

        int orig_idx = static_cast<int>(i) + 1;
        application_argv.push_back(argv[orig_idx]);
        ++i;
    }

    if (inspector_flag) {
        g_setenv("GTK_DEBUG", "interactive", TRUE);
        std::cout << "GTK Inspector enabled." << std::endl;
    }

    if (no_wayland_flag) {
        g_unsetenv("GDK_BACKEND");
        std::cout << "Wayland backend disabled (using default/X11)." << std::endl;
    } else {
        g_setenv("GDK_BACKEND", "wayland", TRUE);
        std::cout << "Using Wayland backend (default)." << std::endl;
    }

    std::cout << "Theme mode set." << std::endl;
    ThemeGuard guard(theme_mode);

    auto app = Gtk::Application::create("org.example.binarytreeeditor");

    auto app_argc = static_cast<int>(application_argv.size());
    char** app_argv = application_argv.data();

    return app->make_window_and_run<EditorWindow>(app_argc, app_argv);
}
