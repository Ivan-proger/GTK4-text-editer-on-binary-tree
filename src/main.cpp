#include <gtkmm.h>
#include "Tree.h"
#include "BinaryTreeFile.h"
#include <fstream>
#include <glib.h>
#include <unistd.h>
#include <filesystem>
#include <iostream>
#include <sstream>

// ----------------------- Theme control -----------------------
enum class ThemeMode {
    LOCAL,      // не трогаем окружение
    ADWAITA,    // GTK_THEME=Adwaita
    IGNORE_USER // игнорируем ~/.config (подменяем XDG_CONFIG_HOME)
};

class ThemeGuard {
public:
    ThemeGuard(ThemeMode mode) : mode_(mode) {
        const char* curXdg = g_getenv("XDG_CONFIG_HOME");
        oldXdg_ = curXdg ? std::string(curXdg) : std::string();

        const char* curTheme = g_getenv("GTK_THEME");
        oldTheme_ = curTheme ? std::string(curTheme) : std::string();

        if (mode_ == ThemeMode::IGNORE_USER) {
            char tmpl[] = "/tmp/bt_editor_cfgXXXXXX";
            char* tmp = mkdtemp(tmpl);
            if (tmp) {
                tmpdir_ = std::string(tmp);
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

private:
    ThemeMode mode_;
    std::string oldXdg_;
    std::string oldTheme_;
    std::string tmpdir_;
};

static size_t count_words(const std::string& s) {
    std::istringstream iss(s);
    size_t cnt = 0;
    std::string w;
    while (iss >> w) ++cnt;
    return cnt;
}

// ----------------------- GUI -----------------------
class EditorWindow : public Gtk::ApplicationWindow {
public:
    EditorWindow() {
        
        // --- ВЕРНУЛ: Применение системной темы ---
        // Это важно для подтягивания ваших кастомных CSS из GNOME
        set_decorated(true);

        apply_system_theme();

        // --- HeaderBar: make it look GNOME-like ---
        m_header_bar.set_show_title_buttons(true);

        m_header_bar.get_style_context()->add_class("titlebar");
        m_header_bar.get_style_context()->add_class("flat"); // Убирает разделительную линию
        m_header_bar.get_style_context()->add_class("background"); // Гарантирует правильный фон

        // right area: search entry (compact)
        auto search = Gtk::make_managed<Gtk::SearchEntry>();
        search->set_hexpand(false);
        m_header_bar.pack_end(*search);

        set_titlebar(m_header_bar);

        // Root container
        m_root.set_orientation(Gtk::Orientation::VERTICAL);
        set_child(m_root);

        // --- Карточка управления (Frame) ---
        auto file_card = Gtk::Frame();
        file_card.set_margin_top(10);
        file_card.set_margin_bottom(5);
        file_card.set_margin_start(10);
        file_card.set_margin_end(10);
        m_root.append(file_card);

        auto file_box = Gtk::Box(Gtk::Orientation::HORIZONTAL, 12);
        file_box.set_margin_top(8);
        file_box.set_margin_bottom(8);
        file_box.set_margin_start(8);
        file_box.set_margin_end(8);
        file_card.set_child(file_box);

        m_file_entry.set_placeholder_text("Path to .bin or .txt file (full path)");
        m_file_entry.set_hexpand(true);
        file_box.append(m_file_entry);

        // --- Кнопки (ваши стили) ---
        m_btn_load_bin.set_label("📂 Load Binary");
        m_btn_save_bin.set_label("💾 Save Binary");
        m_btn_load_txt.set_label("📄 Load Text");
        m_btn_save_txt.set_label("✏️ Save Text");

        // CSS классы сохранены, чтобы системная тема их подхватила
        m_btn_load_bin.get_style_context()->add_class("suggested-action");
        m_btn_save_bin.get_style_context()->add_class("secondary");
        m_btn_load_txt.get_style_context()->add_class("secondary");
        m_btn_save_txt.get_style_context()->add_class("secondary");

        m_btn_load_bin.set_tooltip_text("Load .bin tree file into the editor");
        m_btn_save_bin.set_tooltip_text("Serialize current text into .bin");
        m_btn_load_txt.set_tooltip_text("Load plain text into editor");
        m_btn_save_txt.set_tooltip_text("Save editor text to a plain file");

        file_box.append(m_btn_load_bin);
        file_box.append(m_btn_save_bin);
        file_box.append(m_btn_load_txt);
        file_box.append(m_btn_save_txt);

        // --- Карточка текста (Frame) ---
        auto text_card = Gtk::Frame();
        text_card.set_margin_top(5);
        text_card.set_margin_bottom(10);
        text_card.set_margin_start(10);
        text_card.set_margin_end(10);

        // VEXPAND: Решение проблемы "маленького поля для текста"
        text_card.set_vexpand(true);
        text_card.set_hexpand(true);

        m_root.append(text_card);

        m_scrolled.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
        m_scrolled.set_vexpand(true); // Скролл тоже должен расширяться
        
        text_card.set_child(m_scrolled);
        
        m_textview.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
        m_textview.set_left_margin(5); 
        m_textview.set_right_margin(5);
        
        m_scrolled.set_child(m_textview);

        // --- Статус бар ---
        auto status_box = Gtk::Box(Gtk::Orientation::HORIZONTAL, 8);
        status_box.set_margin_top(6);
        status_box.set_margin_bottom(6);
        status_box.set_margin_start(8);
        status_box.set_margin_end(8);

        Gtk::Label status_icon;
        status_icon.set_text("💾");
        status_box.append(status_icon);

        m_status.set_text("Ready");
        status_box.append(m_status);
        m_root.append(status_box);

        // Signals
        m_btn_load_bin.signal_clicked().connect(sigc::mem_fun(*this, &EditorWindow::on_load_binary));
        m_btn_save_bin.signal_clicked().connect(sigc::mem_fun(*this, &EditorWindow::on_save_binary));
        m_btn_load_txt.signal_clicked().connect(sigc::mem_fun(*this, &EditorWindow::on_load_text));
        m_btn_save_txt.signal_clicked().connect(sigc::mem_fun(*this, &EditorWindow::on_save_text));

        m_file_entry.signal_changed().connect(sigc::mem_fun(*this, &EditorWindow::on_path_entry_changed));
        on_path_entry_changed(); 

        if (auto buf = m_textview.get_buffer()) {
            buf->signal_changed().connect(sigc::mem_fun(*this, &EditorWindow::on_textbuffer_changed));
        }

        m_file_entry.signal_activate().connect(sigc::mem_fun(*this, &EditorWindow::on_file_entry_activate));
        
        set_default_size(950, 700);

        present();
    }

    virtual ~EditorWindow() {}

protected:
    // Это включает поддержку системных настроек темы
    void apply_system_theme() {
        auto settings = Gtk::Settings::get_default();
        if (settings) {
            settings->property_gtk_application_prefer_dark_theme() = true;
        }
    }

    void set_status(const std::string& s) {
        m_status.set_text(s);
    }

    void on_path_entry_changed() {
        auto path = m_file_entry.get_text();
        bool ok = !path.empty();
        m_btn_load_bin.set_sensitive(ok);
        m_btn_save_bin.set_sensitive(ok);
        m_btn_load_txt.set_sensitive(ok);
        m_btn_save_txt.set_sensitive(ok);
    }

    void on_textbuffer_changed() {
        auto buf = m_textview.get_buffer();
        if (!buf) return;
        Glib::ustring t = buf->get_text();
        std::string s = static_cast<std::string>(t);
        size_t chars = s.size();
        size_t words = count_words(s);
        std::ostringstream oss;
        oss << "Chars: " << chars << "  Words: " << words;
        set_status(oss.str());
    }

    void on_file_entry_activate() {
        if (m_btn_load_bin.get_sensitive()) on_load_binary();
    }

    // --- Логика файлов (без изменений) ---
    void on_load_binary() {
        std::string path = m_file_entry.get_text();
        if (path.empty()) { set_status("Provide path..."); return; }
        try {
            BinaryTreeFile bf;
            if (!bf.openFile(path.c_str())) { set_status("Cannot open binary: " + path); return; }
            Tree t;
            bf.loadTree(t);
            char* txt = t.toText();
            if (txt) { m_textview.get_buffer()->set_text(txt); delete[] txt; }
            else m_textview.get_buffer()->set_text("");
            bf.close();
            set_status("Loaded binary: " + path);
        } catch (const std::exception& e) {
            set_status(std::string("Error: ") + e.what());
        }
    }

    void on_save_binary() {
        std::string path = m_file_entry.get_text();
        if (path.empty()) { set_status("Provide path..."); return; }
        try {
            Glib::ustring text = m_textview.get_buffer()->get_text();
            Tree t;
            t.fromText(text.c_str());
            BinaryTreeFile bf;
            if (!bf.openFile(path.c_str())) { set_status("Err open: " + path); return; }
            bf.saveTree(t);
            bf.close();
            set_status("Saved binary: " + path);
        } catch (const std::exception& e) {
            set_status(std::string("Error: ") + e.what());
        }
    }

    void on_load_text() {
        std::string path = m_file_entry.get_text();
        if (path.empty()) { set_status("Provide path..."); return; }
        try {
            std::ifstream in(path, std::ios::binary);
            if (!in) { set_status("Err open txt: " + path); return; }
            std::string s((std::istreambuf_iterator<char>(in)), {});
            m_textview.get_buffer()->set_text(s);
            set_status("Loaded txt: " + path);
        } catch (const std::exception& e) {
            set_status(std::string("Error: ") + e.what());
        }
    }

    void on_save_text() {
        std::string path = m_file_entry.get_text();
        if (path.empty()) { set_status("Provide path..."); return; }
        try {
            Glib::ustring text = m_textview.get_buffer()->get_text();
            std::ofstream out(path, std::ios::binary);
            if (!out) { set_status("Err write txt: " + path); return; }
            out.write(text.data(), text.size());
            set_status("Saved txt: " + path);
        } catch (const std::exception& e) {
            set_status(std::string("Error: ") + e.what());
        }
    }

private:
    // UI Elements
    Gtk::HeaderBar m_header_bar;
    
    Gtk::Box m_root{Gtk::Orientation::VERTICAL};
    
    Gtk::Entry m_file_entry;
    
    Gtk::Button m_btn_load_bin;
    Gtk::Button m_btn_save_bin;
    Gtk::Button m_btn_load_txt;
    Gtk::Button m_btn_save_txt;
    
    Gtk::ScrolledWindow m_scrolled;
    Gtk::TextView m_textview;
    Gtk::Label m_status;
};

int main(int argc, char* argv[]) {
        // Включаем GTK Inspector (вызывается через Ctrl+Shift+D или Ctrl+Shift+I)
    // g_setenv("GTK_DEBUG", "interactive", TRUE);
    g_setenv("GDK_BACKEND", "wayland", TRUE);
    
    ThemeGuard guard(ThemeMode::LOCAL);
    auto app = Gtk::Application::create("org.example.binarytreeeditor");
    
    // ThemeGuard guard(ThemeMode::LOCAL);
    // auto app = Gtk::Application::create("org.example.binarytreeeditor");
    return app->make_window_and_run<EditorWindow>(argc, argv);
}