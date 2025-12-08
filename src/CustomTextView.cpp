// CustomTextView.cpp
#include "CustomTextView.h"
#include <gdk/gdk.h>
#include <glib.h>
#include <pango/pangocairo.h>
#include <algorithm>
#include <cstring>
#include <iostream> // для простого логирования ошибок
#include <cassert>

// --- UTF-8 helpers-------------------------------
// Возвращает индекс lead-byte, который лежит <= pos-1 (т.е. начало символа,
// охватывающего байт pos-1), или 0 если не найдено. Без UB при pos == data_len.
static size_t utf8_prev_boundary(const char* data, size_t data_len, size_t pos) {
    if (data_len == 0) return 0;
    if (pos == 0) return 0;
    if (pos > data_len) pos = data_len;

    // Начинаем с байта перед pos — это безопасно
    size_t i = pos - 1;
    // Если текущий байт continuation (10xxxxxx), сдвигаемся влево до lead-byte
    while (i > 0 && (static_cast<unsigned char>(data[i]) & 0xC0) == 0x80) --i;
    return i;
}

// Возвращает позицию lead-byte для следующего символа (т.е. начало символа,
// который начинается не раньше чем в pos). Если pos уже указывает на lead-byte,
// вернёт pos. Если pos >= data_len — вернёт data_len.
static size_t utf8_next_boundary(const char* data, size_t data_len, size_t pos) {
    if (data_len == 0) return 0;
    if (pos >= data_len) return data_len;
    size_t i = pos;
    // Если pos указывает внутри continuation байтов, продвигаемся вправо
    while (i < data_len && (static_cast<unsigned char>(data[i]) & 0xC0) == 0x80) ++i;
    return i;
}


// === ctor/dtor =============================================================
CustomTextView::CustomTextView() {
    m_font_desc.set_family("Monospace");
    m_font_desc.set_size(10 * PANGO_SCALE);

    set_focusable(true);

    set_draw_func([this](const Cairo::RefPtr<Cairo::Context>& cr, int width, int height){
        draw_with_cairo(cr, width, height);
    });

    // Key controller
    auto key_controller = Gtk::EventControllerKey::create();
    key_controller->signal_key_pressed().connect(sigc::mem_fun(*this, &CustomTextView::on_key_pressed), false);
    add_controller(key_controller);

    // Click gestures for mouse press
    auto click = Gtk::GestureClick::create();
    click->signal_pressed().connect(sigc::mem_fun(*this, &CustomTextView::on_gesture_pressed), false);
    click->signal_released().connect(sigc::mem_fun(*this, &CustomTextView::on_gesture_released), false);
    add_controller(click);

    // ensure flags initial state
    m_mouse_selecting = false;
    m_sel_anchor = -1;
    m_desired_column_px = -1;

    // Motion controller
    auto motion = Gtk::EventControllerMotion::create();
    motion->signal_motion().connect(sigc::mem_fun(*this, &CustomTextView::on_motion), false);
    add_controller(motion);

    // Scroll controller (optional)
    auto scroll = Gtk::EventControllerScroll::create();
    scroll->signal_scroll().connect(sigc::mem_fun(*this, &CustomTextView::on_scroll), false);
    add_controller(scroll);

    // caret blink
    m_caret_timer = Glib::signal_timeout().connect([this]() {
        m_show_caret = !m_show_caret;
        queue_draw();
        return true;
    }, 500);
}

CustomTextView::~CustomTextView() {
    if (m_caret_timer.connected()) m_caret_timer.disconnect();
}


// === public API ============================================================
void CustomTextView::set_tree(Tree* tree) {
    m_tree = tree;
    m_dirty = true;
    reload_from_tree();
}

void CustomTextView::reload_from_tree() {
    if (!m_tree) {
        m_textCache.clear();
        m_lineOffsets.clear();
        update_size_request();
        queue_draw();
        return;
    }
    if (m_tree->getRoot()) {
        char* raw = m_tree->toText();
        if (raw) {
            m_textCache.assign(raw, static_cast<size_t>(::strlen(raw)));
            delete[] raw;
        } else {
            m_textCache.clear();
        }
    } else {
        m_textCache.clear();
    }
    m_dirty = false;
    ensure_text_cache();
    update_size_request();
    queue_draw();
}

void CustomTextView::mark_dirty() {
    m_dirty = true;
    queue_draw();
}

void CustomTextView::set_cursor_byte_offset(int offset) {
    if (offset < 0) offset = 0;
    if (offset > static_cast<int>(m_textCache.size())) offset = static_cast<int>(m_textCache.size());
    m_cursor_byte_offset = offset;
    queue_draw();
}

// Реализация сделана const — как и в заголовке
int CustomTextView::get_cursor_line_index() const {
    if (m_lineOffsets.empty()) return 0;
    int byteOffset = m_cursor_byte_offset;
    auto lo = 0, hi = static_cast<int>(m_lineOffsets.size()) - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (m_lineOffsets[mid] <= byteOffset) lo = mid + 1;
        else hi = mid - 1;
    }
    return std::max(0, hi);
}


// === controllers handlers ================================================
bool CustomTextView::on_key_pressed(guint keyval, guint /*keycode*/, Gdk::ModifierType /*state*/) {
    if (!m_tree) return false;

    // Handle arrows, backspace, delete, enter, printable chars
    if (keyval == GDK_KEY_BackSpace) {
        // если есть выделение — удаляем его целиком
        if (m_sel_start >= 0 && m_sel_len > 0) {
            int s = m_sel_start;
            int l = m_sel_len;
            try {
                if (m_tree) m_tree->erase(s, l);
            } catch (const std::exception& e) {
                std::cerr << "Tree::erase error: " << e.what() << '\n';
            }
            clear_selection();
            reload_from_tree();
            set_cursor_byte_offset(s);
            return true;
        }

        // обычный Backspace (удалить предыдущий символ)
        if (m_cursor_byte_offset > 0) {
            const char* start = m_textCache.c_str();
            const char* curPtr = start + m_cursor_byte_offset;
            const char* prevPtr = g_utf8_prev_char(curPtr);
            int prevOff = static_cast<int>(prevPtr - start);
            int len = m_cursor_byte_offset - prevOff;
            if (len > 0) {
                try {
                    if (m_tree) m_tree->erase(prevOff, len);
                } catch (const std::exception& e) {
                    std::cerr << "Tree::erase error: " << e.what() << '\n';
                }
                reload_from_tree();
                set_cursor_byte_offset(prevOff);
            }
        }
        return true;
    } else if (keyval == GDK_KEY_Delete) {
        if (m_sel_start >= 0 && m_sel_len > 0) {
            int s = m_sel_start;
            int l = m_sel_len;
            try {
                if (m_tree) m_tree->erase(s, l);
            } catch (const std::exception& e) {
                std::cerr << "Tree::erase error: " << e.what() << '\n';
            }
            clear_selection();
            reload_from_tree();
            set_cursor_byte_offset(s);
            return true;
        }
        // иначе обычный Delete - удалить следующий символ
        if (m_cursor_byte_offset < static_cast<int>(m_textCache.size())) {
            const char* start = m_textCache.c_str();
            const char* curPtr = start + m_cursor_byte_offset;
            const char* nextPtr = g_utf8_next_char(curPtr);
            int nextOff = static_cast<int>(nextPtr - start);
            int len = nextOff - m_cursor_byte_offset;
            if (len > 0) {
                try {
                    if (m_tree) m_tree->erase(m_cursor_byte_offset, len);
                } catch (const std::exception& e) {
                    std::cerr << "Tree::erase error: " << e.what() << '\n';
                }
                reload_from_tree();
                set_cursor_byte_offset(m_cursor_byte_offset);
            }
        }
        return true;
    } else if (keyval == GDK_KEY_Left) {
        if (m_cursor_byte_offset > 0) {
            const char* start = m_textCache.c_str();
            const char* curPtr = start + m_cursor_byte_offset;
            const char* prevPtr = g_utf8_prev_char(curPtr);
            auto prevOff = static_cast<int>(prevPtr - start);
            set_cursor_byte_offset(prevOff);
        }
        return true;
    } else if (keyval == GDK_KEY_Right) {
        if (m_cursor_byte_offset < static_cast<int>(m_textCache.size())) {
            const char* start = m_textCache.c_str();
            const char* curPtr = start + m_cursor_byte_offset;
            const char* nextPtr = g_utf8_next_char(curPtr);
            auto nextOff = static_cast<int>(nextPtr - start);
            set_cursor_byte_offset(nextOff);
        }
        return true;
    } else if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
        const char ch = '\n';
        try {
            m_tree->insert(m_cursor_byte_offset, &ch, 1);
        } catch (const std::exception& e) {
            std::cerr << "Tree::insert error: " << e.what() << '\n';
        }
        reload_from_tree();
        set_cursor_byte_offset(m_cursor_byte_offset + 1);
        return true;
    }

    // Printable char: get unicode codepoint from keyval
    gunichar uc = gdk_keyval_to_unicode(keyval);
    if (uc != 0) {
        char buf[6] = {0};
        int bytes = g_unichar_to_utf8(uc, buf);
        try {
            m_tree->insert(m_cursor_byte_offset, buf, bytes);
        } catch (const std::exception& e) {
            std::cerr << "Tree::insert error: " << e.what() << '\n';
        }
        reload_from_tree();
        set_cursor_byte_offset(m_cursor_byte_offset + bytes);
        return true;
    }

    return false; // unhandled -> allow propagation
}

void CustomTextView::on_gesture_released(int /*n_press*/, double /*x*/, double /*y*/) {
    // Завершаем drag-selection
    if (!m_mouse_selecting) return;
    m_mouse_selecting = false;

    // Если ничего не выделено — сбросили якорь
    if (m_sel_start < 0 || m_sel_len == 0) {
        m_sel_anchor = -1;
    } else {
        // оставляем текущее выделение и курсор в его конце
        set_cursor_byte_offset(m_sel_start + m_sel_len);
    }
}


void CustomTextView::on_gesture_pressed(int /*n_press*/, double x, double y) {
    clear_selection();
    grab_focus();

    if (m_dirty) ensure_text_cache();
    if (m_lineOffsets.empty()) {
        set_cursor_byte_offset(0);
        return;
    }

    const int left_margin = 6;
    const int top_margin = 4;

    int clickX = static_cast<int>(x) - left_margin;
    if (clickX < 0) clickX = 0;

    auto lineIndex = static_cast<int>((y - top_margin) / (m_line_height > 0 ? m_line_height : 1));
    if (lineIndex < 0) lineIndex = 0;
    if (m_lineOffsets.empty()) {
        lineIndex = 0;
    } else if (lineIndex >= static_cast<int>(m_lineOffsets.size())) {
        lineIndex = static_cast<int>(m_lineOffsets.size()) - 1;
    }

    int lineStart = m_lineOffsets[lineIndex];
    int lineEnd = (lineIndex + 1 < static_cast<int>(m_lineOffsets.size())) ? m_lineOffsets[lineIndex + 1] - 1 : static_cast<int>(m_textCache.size());
    std::string lineStr;
    if (lineEnd > lineStart) lineStr.assign(m_textCache.data() + lineStart, lineEnd - lineStart);
    else lineStr.clear();

    // Если строка пустая — просто поставим курсор в lineStart
    if (lineStr.empty()) {
        set_cursor_byte_offset(lineStart);
        return;
    }

    // Безопасный путь: не используем Pango при обработке клика (это горячий код событий),
    // а применяем моноширинную аппроксимацию, основанную на m_char_width.
    // Это устраняет падения, связанные с Pango в edge-case'ах.
    //
    // Вычислим примерное число символов до положения клика:
    int approx_chars = clickX / (m_char_width > 0 ? m_char_width : 8);
    if (approx_chars < 0) approx_chars = 0;

    // Пройдём по UTF-8 символам и найдем байтовую позицию, соответствующую approx_chars.
    size_t bpos = 0;
    int chars = 0;
    while (bpos < lineStr.size() && chars < approx_chars) {
        bpos = utf8_next_boundary(lineStr.c_str(), lineStr.size(), bpos);
        ++chars;
    }

    // Точный добор: локально пройдём символы вправо, отслеживая фактическую X позицию
    // на основе m_char_width (позволяет скорректировать в небольших пределах)
    size_t chosenPrefix = bpos;
    int cur_x = chars * (m_char_width > 0 ? m_char_width : 8);
    while (chosenPrefix < lineStr.size()) {
        size_t nextPref = utf8_next_boundary(lineStr.c_str(), lineStr.size(), chosenPrefix);
        if (nextPref == chosenPrefix) break; // safety
        cur_x += (m_char_width > 0 ? m_char_width : 8);
        if (cur_x > clickX) break;
        chosenPrefix = nextPref;
    }

    int newOffset = lineStart + static_cast<int>(chosenPrefix);

    // Начинаем выделение (drag) при удержании кнопки мыши
    m_mouse_selecting = true;
    m_sel_anchor = newOffset;
    // Стартовое выделение отсутствует (нулевая длина)
    select_range_bytes(newOffset, 0);
    set_cursor_byte_offset(newOffset);
}

void CustomTextView::on_motion(double x, double y) {
    // если не в режиме выделения — ничего не делаем
    if (!m_mouse_selecting) return;

    if (m_dirty) ensure_text_cache();
    if (m_lineOffsets.empty()) return;

    const int left_margin = 6;
    const int top_margin = 4;

    int clickX = static_cast<int>(x) - left_margin;
    if (clickX < 0) clickX = 0;

    int lineIndex = static_cast<int>((y - top_margin) / (m_line_height > 0 ? m_line_height : 1));
    if (lineIndex < 0) lineIndex = 0;
    if (lineIndex >= static_cast<int>(m_lineOffsets.size())) lineIndex = static_cast<int>(m_lineOffsets.size()) - 1;

    int lineStart = m_lineOffsets[lineIndex];
    int lineEnd = (lineIndex + 1 < static_cast<int>(m_lineOffsets.size())) ? m_lineOffsets[lineIndex + 1] - 1 : static_cast<int>(m_textCache.size());
    std::string lineStr;
    if (lineEnd > lineStart) lineStr.assign(m_textCache.data() + lineStart, lineEnd - lineStart);
    else lineStr.clear();

    // вычисляем байтовый оффсет в строке при позиции clickX
    size_t chosenPrefix = 0;
    if (lineStr.empty()) {
        chosenPrefix = 0;
    } else {
        int approx_chars = clickX / (m_char_width > 0 ? m_char_width : 8);
        if (approx_chars < 0) approx_chars = 0;

        size_t bpos = 0;
        int chars = 0;
        while (bpos < lineStr.size() && chars < approx_chars) {
            bpos = utf8_next_boundary(lineStr.c_str(), lineStr.size(), bpos);
            ++chars;
        }
        chosenPrefix = bpos;
        int cur_x = chars * (m_char_width > 0 ? m_char_width : 8);
        while (chosenPrefix < lineStr.size()) {
            size_t nextPref = utf8_next_boundary(lineStr.c_str(), lineStr.size(), chosenPrefix);
            if (nextPref == chosenPrefix) break;
            cur_x += (m_char_width > 0 ? m_char_width : 8);
            if (cur_x > clickX) break;
            chosenPrefix = nextPref;
        }
    }

    int currentOffset = lineStart + static_cast<int>(chosenPrefix);

    // Обновляем выделение между якорем и текущей позицией
    if (m_sel_anchor < 0) {
        m_sel_anchor = currentOffset; // safety
    }
    int selBeg = std::min(m_sel_anchor, currentOffset);
    int selEnd = std::max(m_sel_anchor, currentOffset);
    select_range_bytes(selBeg, selEnd - selBeg);

    // не ставим курсор в середину выделения — курсор в конце выделения
    set_cursor_byte_offset(currentOffset);
}


bool CustomTextView::on_scroll(double /*dx*/, double /*dy*/) {
    // not intercepted
    return false;
}


// === drawing / cache ======================================================
void CustomTextView::ensure_text_cache() {
    m_lineOffsets.clear();
    const char* s = m_textCache.c_str();
    auto len = static_cast<int>(m_textCache.size());
    m_lineOffsets.push_back(0);
    for (int i = 0; i < len; ++i) {
        if (s[i] == '\n') {
            if (i + 1 <= len) m_lineOffsets.push_back(i + 1);
        }
    }
    if (m_lineOffsets.empty()) m_lineOffsets.push_back(0);
    recompute_metrics();
    m_dirty = false;
}

void CustomTextView::recompute_metrics() {
    auto layout = create_pango_layout("X");
    layout->set_font_description(m_font_desc);
    int w = 0, h = 0;
    layout->get_pixel_size(w, h);
    m_line_height = h > 0 ? h : 16;

    layout->set_text("M");
    layout->get_pixel_size(w, h);
    m_char_width = w > 0 ? w : 8;
}

void CustomTextView::update_size_request() {
    auto total_lines = static_cast<int>(m_lineOffsets.size());
    if (total_lines == 0) total_lines = 1;
    int total_height = total_lines * m_line_height + 10;
    set_size_request(-1, total_height);
}

int CustomTextView::measure_prefix_pixels_in_line(const std::string& line, size_t bytePrefixLen) {
    if (bytePrefixLen > line.size()) bytePrefixLen = line.size();

    // Если виджет ещё не реализован/не готов — используем безопасный моноширинный fallback.
    // Это исключит обращения в Pango, которые в некоторых edge-case'ах могут падать.
    if (!get_realized()) {
        // посчитаем число юникод-символов (лидеров UTF-8) в префиксе
        int chars = 0;
        for (size_t i = 0; i < bytePrefixLen; ++i) {
            unsigned char c = static_cast<unsigned char>(line[i]);
            if ((c & 0xC0) != 0x80) ++chars;
        }
        return chars * m_char_width;
    }

    // Если виджет реализован — используем Pango для точного измерения.
    auto layout = create_pango_layout(line);
    layout->set_font_description(m_font_desc);

    PangoRectangle strong_pos;
    pango_layout_get_cursor_pos(layout->gobj(), static_cast<int>(bytePrefixLen), &strong_pos, nullptr);
    return strong_pos.x / PANGO_SCALE;
}



void CustomTextView::draw_with_cairo(const Cairo::RefPtr<Cairo::Context>& cr, int /*width*/, int /*height*/) {
    if (m_dirty) ensure_text_cache();

    // background
    try {
        Gdk::RGBA bg;
        auto ctx = get_style_context();
        bg.set("background");
        ctx->add_class("background");
        cr->set_source_rgba(bg.get_red(), bg.get_green(), bg.get_blue(), bg.get_alpha());
        cr->paint();
    } catch(...) {
        cr->set_source_rgb(1.0, 1.0, 1.0);
        cr->paint();
    }

    const int left_margin = 6;
    const int top_margin = 4;

    size_t nlines = m_lineOffsets.size();
    for (size_t i = 0; i < nlines; ++i) {
        int start = m_lineOffsets[i];
        int end = (i + 1 < nlines) ? m_lineOffsets[i + 1] - 1 : static_cast<int>(m_textCache.size());
        if (end < start) end = start;
        int len = end - start;

        // текст линии в байтах
        std::string line_tmp(m_textCache.data() + start, static_cast<size_t>(len));

        // --- selection drawing for this line ---
        bool has_selection_this_line = false;
        int sel_L = 0, sel_R = 0; // в байтах относительно начала строки
        if (m_sel_start >= 0 && m_sel_len > 0) {
            int sel_beg = m_sel_start;
            int sel_end = m_sel_start + m_sel_len; // exclusive
            int L = std::max(start, sel_beg);
            int R = std::min(end, sel_end);
            if (R > L) {
                has_selection_this_line = true;
                sel_L = L - start;
                sel_R = R - start;

                // измерим X начала и конца (в пикселях) внутри этой строки
                int x1 = left_margin + measure_prefix_pixels_in_line(line_tmp, static_cast<size_t>(sel_L));
                int x2 = left_margin + measure_prefix_pixels_in_line(line_tmp, static_cast<size_t>(sel_R));
                int y = top_margin + static_cast<int>(i) * m_line_height;
                double sel_a = 0.35; // прозрачность

                Gdk::RGBA sel_color;
                sel_color.set_red(0.15);
                sel_color.set_green(0.45);
                sel_color.set_blue(0.85);
                sel_color.set_alpha(sel_a);

                cr->save();
                cr->set_source_rgba(sel_color.get_red(), sel_color.get_green(), sel_color.get_blue(), sel_color.get_alpha());
                cr->rectangle(x1, y, x2 - x1, m_line_height);
                cr->fill();
                cr->restore();
            }
        }

        // --- DRAW TEXT: используем один Pango::Layout на строку ---
        auto layout = create_pango_layout(line_tmp);
        layout->set_font_description(m_font_desc);

        // Цвет текста по умолчанию (попытаемся взять из темы, иначе чёрный)
        Gdk::RGBA fg_color;
        try {
            auto ctx = get_style_context();
            fg_color = ctx->get_color();
        } catch(...) {
            fg_color.set_red(0.0);
            fg_color.set_green(0.0);
            fg_color.set_blue(0.0);
            fg_color.set_alpha(1.0);
        }

        Gdk::RGBA sel_fg;
        sel_fg.set_red(1.0);
        sel_fg.set_green(1.0);
        sel_fg.set_blue(1.0);
        sel_fg.set_alpha(1.0);

        // helper: рисует фрагмент строки, зная его байтовую позицию внутри строки
        auto draw_fragment_by_bytes = [&](size_t frag_start, size_t frag_len, const Gdk::RGBA& color) {
            if (frag_len == 0) return;
            if (frag_start >= line_tmp.size()) return;
            if (frag_start + frag_len > line_tmp.size()) frag_len = line_tmp.size() - frag_start;

            // pango_layout_get_substring нет в C API, поэтому используем set_text на временном layout.
            std::string frag(line_tmp.data() + frag_start, frag_len);
            auto frag_layout = create_pango_layout(frag);
            frag_layout->set_font_description(m_font_desc);

            int offset_x = left_margin + measure_prefix_pixels_in_line(line_tmp, frag_start);
            int y = top_margin + static_cast<int>(i) * m_line_height;

            cr->save();
            cr->translate(offset_x, y);
            cr->set_source_rgba(color.get_red(), color.get_green(), color.get_blue(), color.get_alpha());
            pango_cairo_show_layout(cr->cobj(), frag_layout->gobj());
            cr->restore();
        };

        if (!has_selection_this_line) {
            // просто весь текст
            draw_fragment_by_bytes(0, line_tmp.size(), fg_color);
        } else {
            // before
            draw_fragment_by_bytes(0, static_cast<size_t>(sel_L), fg_color);
            // selected
            draw_fragment_by_bytes(static_cast<size_t>(sel_L), static_cast<size_t>(sel_R - sel_L), sel_fg);
            // after
            draw_fragment_by_bytes(static_cast<size_t>(sel_R), (line_tmp.size() - sel_R), fg_color);
        }
    } // конец цикла по строкам

    // caret
    if (m_show_caret) {
        int byteOffset = m_cursor_byte_offset;
        if (byteOffset < 0) byteOffset = 0;
        if (byteOffset > static_cast<int>(m_textCache.size())) byteOffset = static_cast<int>(m_textCache.size());
        int lineIndex = static_cast<int>(std::upper_bound(m_lineOffsets.begin(), m_lineOffsets.end(), byteOffset) - m_lineOffsets.begin() - 1);
        if (lineIndex < 0) lineIndex = 0;
        if (m_lineOffsets.empty()) {
            lineIndex = 0;
        } else if (lineIndex >= static_cast<int>(m_lineOffsets.size())) {
            lineIndex = static_cast<int>(m_lineOffsets.size()) - 1;
        }
        int lineStart = m_lineOffsets[lineIndex];
        int prefixLen = byteOffset - lineStart;
        if (prefixLen < 0) prefixLen = 0;
        int lineEnd = (lineIndex + 1 < static_cast<int>(m_lineOffsets.size())) ? m_lineOffsets[lineIndex + 1] - 1 : static_cast<int>(m_textCache.size());
        std::string lineBytes;
        if (lineEnd > lineStart) lineBytes.assign(m_textCache.data() + lineStart, lineEnd - lineStart);
        else lineBytes.clear();
        int cursor_x = left_margin + measure_prefix_pixels_in_line(lineBytes, prefixLen);
        int cursor_y = top_margin + lineIndex * m_line_height;
        cr->save();
        cr->set_source_rgb(0.0, 0.0, 0.0);
        cr->rectangle(cursor_x, cursor_y, 1.0, m_line_height);
        cr->fill();
        cr->restore();
    }
}


// === selection / misc =====================================================
void CustomTextView::select_range_bytes(int startByte, int lengthBytes) {
    if (startByte < 0) startByte = 0;
    int maxb = static_cast<int>(m_textCache.size());
    if (startByte > maxb) startByte = maxb;

    if (lengthBytes <= 0) {
        m_sel_start = -1;
        m_sel_len = 0;
        queue_draw();
        return;
    }

    int endByte = startByte + lengthBytes;
    if (endByte > maxb) endByte = maxb;

    const char* data = m_textCache.data();
    size_t data_len = static_cast<size_t>(maxb);

    size_t sb = static_cast<size_t>(startByte);
    size_t eb = static_cast<size_t>(endByte);

    size_t safe_sb = utf8_prev_boundary(data, data_len, sb);
    size_t safe_eb = utf8_next_boundary(data, data_len, eb);

    if (safe_eb <= safe_sb) {
        m_sel_start = -1;
        m_sel_len = 0;
    } else {
        m_sel_start = static_cast<int>(safe_sb);
        m_sel_len = static_cast<int>(safe_eb - safe_sb);
    }
    queue_draw();
}

void CustomTextView::clear_selection() {
    m_sel_start = -1;
    m_sel_len = 0;
    queue_draw();
}

void CustomTextView::scroll_to_byte_offset(int byteOffset) {
    if (m_dirty) ensure_text_cache();
    if (m_lineOffsets.empty()) {
        // ничего не делаем, т.к. нет данных для скролла
        return;
    }
    if (byteOffset < 0) byteOffset = 0;
    if (byteOffset > static_cast<int>(m_textCache.size())) byteOffset = static_cast<int>(m_textCache.size());

    int lineIndex = 0;
    if (!m_lineOffsets.empty()) {
        auto it = std::upper_bound(m_lineOffsets.begin(), m_lineOffsets.end(), byteOffset);
        lineIndex = static_cast<int>(std::distance(m_lineOffsets.begin(), it)) - 1;
        if (lineIndex < 0) lineIndex = 0;
    }

    int y = lineIndex * m_line_height;

    Gtk::Widget* w = this;
    Gtk::ScrolledWindow* found_sw = nullptr;
    Gtk::Window* found_win = nullptr;

    while (w) {
        auto parent = w->get_parent();
        if (!parent) {
            found_win = dynamic_cast<Gtk::Window*>(w);
            break;
        }
        found_sw = dynamic_cast<Gtk::ScrolledWindow*>(parent);
        if (found_sw) {
            Gtk::Widget* up = parent;
            while (up) {
                auto pup = up->get_parent();
                if (!pup) { found_win = dynamic_cast<Gtk::Window*>(up); break; }
                up = pup;
            }
            break;
        }
        w = parent;
    }

    if (found_sw) {
        if (auto vadj = found_sw->get_vadjustment()) {
            int maxv = static_cast<int>(vadj->get_upper() - vadj->get_page_size());
            if (y < 0) y = 0;
            if (y > maxv) y = maxv;
            vadj->set_value(y);
        }
        return;
    }

    if (found_win) {
        return;
    }
    // Ничего не найдено — молча выходим
}
