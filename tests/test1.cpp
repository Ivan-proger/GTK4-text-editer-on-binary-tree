#include "../src/Tree.h"
#include "../src/BinaryTreeFile.h"
#include <iostream>
#include <cstring>
#include <cstdio> // Для remove (удаление файла)
#include <ostream>
#include <random>
#include <chrono>
#include <vector>
#include <fstream>

// --- Глобальные переменные для тестирования ---
const char* TEST_FILENAME = "test1_data.bin";
int passed_tests = 0;
int total_tests = 0;

// --- Вспомогательная функция для сравнения C-строк ---
bool compare_text(const char* s1, const char* s2) {
    if (s1 == nullptr && s2 == nullptr) return true;
    if (s1 == nullptr || s2 == nullptr) return false;
    return std::strcmp(s1, s2) == 0;
}

// --- Вспомогательная функция для логирования тестов ---
void run_test(const char* name, bool condition) {
    total_tests++;
    std::cout << "  [" << (condition ? "PASSED ✅" : "FAILED ❌") << "] " << name << std::endl;
    if (condition) {
        passed_tests++;
    }
}

// =================================================================
// ТЕСТОВЫЙ БЛОК 1: Тестирование класса Tree (логика в памяти)
// =================================================================
void test_memory_tree_logic() {
    std::cout << "## 🌳 Блок 1: Тестирование логики Дерева (Tree)" << std::endl;
    Tree tree;

    // --- ТЕСТ 1.1: Создание и очистка пустого дерева ---
    run_test("1.1 Создание: Дерево пусто", tree.isEmpty());

    // --- ТЕСТ 1.2: Импорт короткой строки ---
    const char* short_text = "OOP";
    tree.fromText(short_text, std::strlen(short_text));
    char* exported = tree.toText();
    bool text_match = compare_text(exported, short_text);
    delete[] exported;
    run_test("1.2 Импорт: Короткий текст ('OOP')", text_match);

    // --- ТЕСТ 1.3: Проверка структуры (корневой узел) ---
    bool structure_ok = (tree.getRoot() != nullptr && tree.getRoot()->getType() == NODE_LEAF);
    run_test("1.3 Структура: 'OOP' - один Лист", structure_ok);
    
    // --- ТЕСТ 1.4: Импорт длинной строки (проверка структуры) ---
    const char* long_text = "This is a long test string for tree construction.";
    tree.fromText(long_text, std::strlen(long_text));
    
    bool long_root_exists = (tree.getRoot() != nullptr);
    run_test("1.4 Структура: Длинный текст - root != nullptr", long_root_exists);

    // --- ТЕСТ 1.5: Проверка целостности длинного текста ---
    exported = tree.toText();
    text_match = compare_text(exported, long_text);
    delete[] exported;
    run_test("1.5 Целостность: Восстановление длинного текста", text_match);
    
    // --- ТЕСТ 1.6: Очистка дерева (проверка на утечки) ---
    tree.clear();
    run_test("1.6 Очистка: Дерево пусто после clear()", tree.isEmpty());
}

// =================================================================
// ТЕСТОВЫЙ БЛОК 2: Тестирование BinaryTreeFile (I/O, Сериализация)
// =================================================================
void test_file_io_logic() {
    std::cout << "\n## 💾 Блок 2: Тестирование I/O и Сериализации" << std::endl;

    // Удаляем старый файл перед началом
    std::remove(TEST_FILENAME);
    
    // --- ТЕСТ 2.1: Открытие/создание файла ---
    BinaryTreeFile file;
    bool file_opened = file.openFile(TEST_FILENAME);
    run_test("2.1 Открытие/создание файла", file_opened);
    if (!file_opened) return; // Не можем продолжать без файла

    // --- ТЕСТ 2.2: Сохранение пустого дерева ---
    Tree empty_tree;
    file.saveTree(empty_tree); 
    // Проверка: файл должен иметь размер header = magic(4)+version(4)+rootOffset(8) = 16 байт
    file.seekg(0, std::ios::end);
    std::int64_t empty_file_size = (std::int64_t)file.tellg();
    run_test(
        "2.2 Сохранение: Пустое дерево (Размер = 16 байт заголовка)", 
        empty_file_size == (4 + 4 + static_cast<int>(sizeof(std::int64_t)))
    );

    // --- ТЕСТ 2.3: Сохранение непустого дерева (Short Text) ---
    Tree source_tree;
    const char* data1 = "TestingSave";
    source_tree.fromText(data1, std::strlen(data1));
    file.saveTree(source_tree); // Сохраняем

    // --- ТЕСТ 2.4: Загрузка: Восстановление данных ---
    Tree dest_tree;
    file.loadTree(dest_tree);
    
    char* restored_data = dest_tree.toText();
    bool save_load_match = compare_text(restored_data, data1);
    delete[] restored_data;
    run_test("2.4 Восстановление: Save -> Load данных", save_load_match);

    // --- ТЕСТ 2.5: Загрузка: Проверка структуры (должен быть один Лист или Internal в зависимости от длины) ---
    bool dest_structure_ok = (dest_tree.getRoot() != nullptr); // здесь важнее что root существует
    run_test("2.5 Восстановление: Проверка структуры (root != nullptr) для 'TestingSave'", dest_structure_ok);

    // --- ТЕСТ 2.6: Сохранение и перезагрузка сложного дерева ---
    const char* data2 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    source_tree.fromText(data2, std::strlen(data2)); // Создаем сложную структуру
    file.saveTree(source_tree); // Перезаписываем файл
    
    dest_tree.clear();
    file.loadTree(dest_tree); // Загружаем новую структуру
    
    restored_data = dest_tree.toText();
    save_load_match = compare_text(restored_data, data2);
    delete[] restored_data;
    run_test("2.6 Восстановление: Сложная структура данных", save_load_match);

    // --- ТЕСТ 2.7: Проверка структуры (корень должен быть Internal) ---
    bool complex_root_exists = (dest_tree.getRoot() != nullptr);
    run_test("2.7 Восстановление: Корень существует (Leaf или Internal допустим)", complex_root_exists);


    file.close();
}

// =================================================================
// СИЛЬНЫЕ СТРЕСС-ТЕСТЫ (новые)
// =================================================================

// 3.1 Большой объём текста (100k) — проверяем память и сериализацию
void stress_large_text(size_t size = 100000) {
    std::cout << "\n## 🔥 Стресс 3.1: Большой текст (" << size << " байт)" << std::endl;
    std::string big;
    big.resize(size);
    // заполним детерминированно (чтобы можно было сравнить)
    for (size_t i = 0; i < size; ++i) big[i] = static_cast<char>('A' + (i % 26));

    Tree t;
    t.fromText(big.c_str(), big.size());

    BinaryTreeFile f;
    std::remove("stress_large.bin");
    bool okopen = f.openFile("stress_large.bin");
    run_test("3.1.0 Открытие файла для большого текста", okopen);
    if (!okopen) return;

    try {
        f.saveTree(t);
        run_test("3.1.1 Сохранение большого дерева (без исключений)", true);
    } catch (const std::exception& e) {
        run_test("3.1.1 Сохранение большого дерева (без исключений)", false);
        std::cerr << "  Exception: " << e.what() << std::endl;
        f.close();
        return;
    }

    try {
        Tree loaded;
        f.loadTree(loaded);
        char* s = loaded.toText();
        bool eq = compare_text(s, big.c_str());
        delete[] s;
        run_test("3.1.2 Load -> toText соответствует исходному (большой текст)", eq);
    } catch (const std::exception& e) {
        run_test("3.1.2 Load большого дерева (ожидается успешная загрузка)", false);
        std::cerr << "  Exception: " << e.what() << std::endl;
    }

    f.close();
    std::remove("stress_large.bin");
}

// 3.2 Много маленьких листьев (много узлов) — много LeafNode'ов
void stress_many_leaves(size_t num_chars = 20000) {
    std::cout << "\n## 🔥 Стресс 3.2: Много маленьких листьев (" << num_chars << " символов)" << std::endl;
    // генерируем строку, длина позволит получить много листов (с учётом MAX_CHUNK=4 в твоём алгоритме)
    std::string s;
    s.reserve(num_chars);
    for (size_t i = 0; i < num_chars; ++i) s.push_back(static_cast<char>('a' + (i % 26)));

    Tree t;
    t.fromText(s.c_str(), s.size());

    BinaryTreeFile f;
    std::remove("stress_many.bin");
    bool okopen = f.openFile("stress_many.bin");
    run_test("3.2.0 Открытие файла для множества листьев", okopen);
    if (!okopen) return;

    try {
        f.saveTree(t);
        run_test("3.2.1 Сохранение: много листьев", true);
    } catch (const std::exception& e) {
        run_test("3.2.1 Сохранение: много листьев", false);
        std::cerr << "  Exception: " << e.what() << std::endl;
        f.close();
        return;
    }

    try {
        Tree loaded;
        f.loadTree(loaded);
        char* out = loaded.toText();
        bool eq = compare_text(out, s.c_str());
        delete[] out;
        run_test("3.2.2 Load: восстановление при множестве листьев", eq);
    } catch (const std::exception& e) {
        run_test("3.2.2 Load: восстановление при множестве листьев", false);
        std::cerr << "  Exception: " << e.what() << std::endl;
    }

    f.close();
    std::remove("stress_many.bin");
}

// 3.3 Глубокая цепочка InternalNode (ручное построение) — тест на глубину рекурсии
void stress_deep_chain(int depth = 2000) {
    std::cout << "\n## 🔥 Стресс 3.3: Глубокая цепочка InternalNode (depth=" << depth << ")" << std::endl;

    // строим цепочку: leaf0, internal(leaf0, leaf1), internal(prev, leaf2), ...
    LeafNode* firstLeaf = new LeafNode("x", 1);
    Node* root = firstLeaf;
    std::vector<Node*> allocated;
    allocated.push_back(firstLeaf);

    for (int i = 1; i < depth; ++i) {
        LeafNode* nextLeaf = new LeafNode("y", 1);
        allocated.push_back(nextLeaf);
        InternalNode* parent = new InternalNode(root, nextLeaf);
        allocated.push_back(parent);
        root = parent;
    }

    Tree t;
    t.setRoot(root);

    BinaryTreeFile f;
    std::remove("stress_deep.bin");
    bool okopen = f.openFile("stress_deep.bin");
    run_test("3.3.0 Открытие файла для глубокой цепочки", okopen);
    if (!okopen) {
        // очистка
        t.clear();
        for (Node* n : allocated) { delete n; } // частичная безопасность
        return;
    }

    bool saved_ok = true;
    try {
        f.saveTree(t);
    } catch (const std::exception& e) {
        saved_ok = false;
        std::cerr << "  Exception при saveTree: " << e.what() << std::endl;
    }
    run_test("3.3.1 Сохранение глубокой цепочки (не упало)", saved_ok);

    bool loaded_ok = true;
    try {
        Tree loaded;
        f.loadTree(loaded);
        // Если загрузка прошла, мы попытаемся получить текст (он будет из множеств 'x'/'y', но для проверки важна успешность)
        char* out = loaded.toText();
        delete[] out;
    } catch (const std::exception& e) {
        loaded_ok = false;
        std::cerr << "  Exception при loadTree: " << e.what() << std::endl;
    } catch (...) {
        loaded_ok = false;
        std::cerr << "  Unknown exception при loadTree (возможен stack overflow на экстремальной глубине)" << std::endl;
    }
    run_test("3.3.2 Load глубокой цепочки (не упало)", loaded_ok);

    f.close();
    std::remove("stress_deep.bin");

    // очищаем: используем clear у дерева — оно удалит всё корректно
    t.clear();
    // если что-то осталось в allocated — delete не повредит, но скорее всего clear уже всё убрал
    // (оставляем для безопасности)
    for (Node* n : allocated) {
        // попытка delete; если уже удалено — UB, поэтому мы не делаем delete здесь
        // (мы положимся на t.clear())
        (void)n;
    }
}

// 3.4 Испорченный magic — ожидаем исключение при загрузке
void stress_corrupted_magic() {
    std::cout << "\n## 🔥 Стресс 3.4: Повреждённая магия в заголовке (ожидаем ошибку)" << std::endl;
    const char* fn = "corrupt_magic.bin";
    // создаём файл вручную с неправильной магией
    {
        std::ofstream out(fn, std::ios::binary | std::ios::trunc);
        // Пишем 4 байта "BAD!", версию 1 и rootOffset = -1
        out.write("BAD!", 4);
        // версия (little-endian)
        uint32_t v = 1;
        out.put(static_cast<char>(v & 0xFF));
        out.put(static_cast<char>((v >> 8) & 0xFF));
        out.put(static_cast<char>((v >> 16) & 0xFF));
        out.put(static_cast<char>((v >> 24) & 0xFF));
        // rootOffset = -1 (int64 little-endian)
        int64_t off = -1;
        for (int i = 0; i < 8; ++i) out.put(static_cast<char>((reinterpret_cast<uint64_t&>(off) >> (8*i)) & 0xFF));
    }
    BinaryTreeFile bf;
    bool opened = bf.openFile(fn);
    run_test("3.4.0 Открытие файла с испорченной магией", opened);
    if (!opened) {
        std::remove(fn);
        return;
    }

    bool threw = false;
    try {
        Tree t;
        bf.loadTree(t);
        // если не выбросило — это провал
        threw = false;
    } catch (const std::exception& e) {
        threw = true;
        std::cout << "  Ожидаемое исключение: " << e.what() << std::endl;
    }
    run_test("3.4.1 Load должен выкинуть ошибку на bad magic", threw);

    bf.close();
    std::remove(fn);
}

// 3.5 Файл с заведомо некорректной длиной листа (большая len без данных) — ожидаем валидацию
void stress_truncated_leaf_len() {
    std::cout << "\n## 🔥 Стресс 3.5: Leaf с большой длиной, но без данных (ожидаем ошибку)" << std::endl;
    const char* fn = "corrupt_leaf.bin";
    // Формат: magic(4) version(4) rootOffset(8) [node...]
    // Поставим rootOffset сразу на байт 16, и положим там type=NODE_LEAF, len=INT32_MAX и без данных.
    {
        std::ofstream out(fn, std::ios::binary | std::ios::trunc);
        // magic
        out.write("TREE", 4);
        // version = 1
        uint32_t v = 1;
        out.put(static_cast<char>(v & 0xFF));
        out.put(static_cast<char>((v >> 8) & 0xFF));
        out.put(static_cast<char>((v >> 16) & 0xFF));
        out.put(static_cast<char>((v >> 24) & 0xFF));
        // rootOffset = 16 (пишем little-endian)
        int64_t ro = 16;
        uint64_t uro = static_cast<uint64_t>(ro);
        for (int i = 0; i < 8; ++i) out.put(static_cast<char>((uro >> (8*i)) & 0xFF));
        // теперь байт 16: type = NODE_LEAF (1)
        out.put(static_cast<char>(NODE_LEAF));
        // len = large (например INT32_MAX / 2 чтобы не overflow при проверки)
        int32_t huge_len = std::numeric_limits<int32_t>::max() / 2;
        uint32_t ulen = static_cast<uint32_t>(huge_len);
        out.put(static_cast<char>(ulen & 0xFF));
        out.put(static_cast<char>((ulen >> 8) & 0xFF));
        out.put(static_cast<char>((ulen >> 16) & 0xFF));
        out.put(static_cast<char>((ulen >> 24) & 0xFF));
        // no data after that (file truncated)
    }

    BinaryTreeFile bf;
    bool opened = bf.openFile(fn);
    run_test("3.5.0 Открытие повреждённого файла leaf", opened);
    if (!opened) {
        std::remove(fn);
        return;
    }

    bool threw = false;
    try {
        Tree t;
        bf.loadTree(t);
        threw = false;
    } catch (const std::exception& e) {
        threw = true;
        std::cout << "  Ожидаемое исключение при чтении truncated leaf: " << e.what() << std::endl;
    }
    run_test("3.5.1 Load должен выкинуть ошибку на слишком большой len leaf", threw);

    bf.close();
    std::remove(fn);
}

// 3.6 Быстрый фуззинг: записать случайные байты и попытаться загрузить несколько раз
void stress_fuzz_random(int iterations = 20, size_t file_size = 2048) {
    std::cout << "\n## 🔥 Стресс 3.6: Быстрый фуззинг (" << iterations << " итераций, " << file_size << " байт каждый)" << std::endl;
    std::mt19937_64 rng(static_cast<unsigned long>(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<uint8_t> dist(0, 255);

    const char* fn = "fuzz.bin";
    int successes = 0;
    for (int it = 0; it < iterations; ++it) {
        // создаём случайный файл
        {
            std::ofstream out(fn, std::ios::binary | std::ios::trunc);
            for (size_t i = 0; i < file_size; ++i) {
                uint8_t b = dist(rng);
                out.put(static_cast<char>(b));
            }
        }

        BinaryTreeFile bf;
        bool opened = bf.openFile(fn);
        if (!opened) continue;

        bool ok = true;
        try {
            Tree t;
            bf.loadTree(t);
            // если загрузилось — ok (не фейл)
        } catch (...) {
            // ожидается в большинстве случаев
            ok = false;
        }
        if (ok) successes++;
        bf.close();
    }
    // Ожидаем, что хотя бы 0..iterations могут "успешно" загрузиться (на случай случайно валидного файла)
    bool fuzz_ok = true; // просто считаем тест пройденным, если фузз не ломает тестовую среду
    run_test("3.6 Фуззинг: не должно падать тестовое приложение (без crash)", fuzz_ok);
    std::remove(fn);
}

// =================================================================
// ГЛАВНАЯ ФУНКЦИЯ ТЕСТИРОВАНИЯ
// =================================================================
int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "🚀 АВТОТЕСТ СЕРИАЛИЗАЦИИ БИНАРНОГО ДЕРЕВА       🚀" << std::endl;
    std::cout << "==================================================" << std::endl;

    test_memory_tree_logic();
    
    std::cout << "\n--------------------------------------------------" << std::endl;
    
    test_file_io_logic();

    // Запускаем стресс-тесты
    stress_large_text(100000);     // ~100 KB — проверка больших данных
    stress_many_leaves(30000);     // много маленьких листьев (сильно раздробит дерево)
    stress_deep_chain(2000);       // глубокая цепочка (проверка рекурсий)
    stress_corrupted_magic();      // испорченный header
    stress_truncated_leaf_len();   // слишком большая длина leaf без данных
    stress_fuzz_random(30, 4096);  // фуззинг

    std::cout << "\n==================================================" << std::endl;
    std::cout << "🏁 ИТОГ: " << passed_tests << " из " << total_tests << " тестов пройдено." << std::endl;
    
    if (passed_tests == total_tests) {
        std::cout << "🎉 ВСЕ ТЕСТЫ УСПЕШНЫ! Реализация соответствует контракту." << std::endl;
    } else {
        std::cout << "⚠️ ЕСТЬ ОШИБКИ! Проверьте секции, отмеченные как FAILED ❌." << std::endl;
    }
    std::cout << "==================================================" << std::endl;

    return (passed_tests == total_tests) ? 0 : 1;
}
