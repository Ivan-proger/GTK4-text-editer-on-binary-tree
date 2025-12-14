#ifndef TREE_H
#define TREE_H


//! КРАЙ ПО КОТОРОМУ РЕЖЕТСЯ ЛИСТ - НЕКОРРЕКТНОЕ ПОВЕДЕНИЕ ПОСЛЕ ПОКА ЧТО ПРОСТО ЗАГЛУШКА НЕ ВАЖНО
//! ПОСЛЕ ПОКА ЧТО ПРОСТО ЗАГЛУШКА НЕ ВАЖНО
const int MAX_LEAF_SIZE = 4096; //TODO: фикс

enum class NodeType : char {
    NODE_INTERNAL = 0,
    NODE_LEAF = 1
};

struct Node {
    virtual NodeType getType() const = 0;

    // Быстрый доступ к статистике
    virtual int getLength() const = 0; // Вес в байтах
    virtual int getLineCount() const = 0; // Вес в строках (\n)

    virtual ~Node() = default;
};

struct LeafNode : public Node {
    int length;
    int lineCount; // Количество строк-1 (\n)
    char* data; // Указатель на строку в памяти (кучи)

    LeafNode(const char* str, int len);
    ~LeafNode() override;

    // Запрет копирования (от утечек)
    LeafNode(const LeafNode&) = delete; 
    // Запрет присваивания копированием (от утечек)
    LeafNode& operator=(const LeafNode&) = delete;

    // Реализуем перемещающий конструктор и перемещающее присваивание:
    LeafNode(LeafNode&& other) noexcept;
    LeafNode& operator=(LeafNode&& other) noexcept;

    NodeType getType() const override;
    int getLength() const override;
    int getLineCount() const override;
};

struct InternalNode : public Node {
    Node* left;
    Node* right;

    // Суммы детей
    int totalLength;
    int totalLineCount;

    InternalNode(Node* l, Node* r);
    ~InternalNode() override = default;

    NodeType getType() const override;
    int getLength() const override;
    int getLineCount() const override;

    void recalc(); // пересчитать totalLength и totalLineCount
};

class Tree {
private:
    Node* root;

    void clearRecursive(Node* node);
    Node* buildFromTextRecursive(const char* text, int len);
    
    // Вспомогательная рекурсия для сбора текста (теперь проще)
    void collectTextRecursive(Node* node, char* buffer, int& pos);

    // Вспомогательная функция для поиска листа по номеру строки
    // Изменяет localLineIndex, приводя его к индексу внутри найденного листа
    LeafNode* findLeafByLineRecursive(Node* node, int& localLineIndex);

    LeafNode* findLeafByOffsetRecursive(Node* node, int& localOffset);
    Node* splitLeafAtOffset(LeafNode* leaf, int offset);

    Node* insertIntoLeaf(LeafNode* leaf, int pos, const char* data, int len);
    int findSplitIndexForLeaf(const LeafNode* leaf) const;
    // Рекурсивные реализации вставки/удаления (возвращают новый Node* для замены в родителе)
    Node* insertRecursive(Node* node, int pos, const char* data, int len);

    // Удалить len байт в листе, возвращает новый Node* (новый лист или nullptr)
    Node* eraseFromLeaf(LeafNode* leaf, int pos, int len);

    // Если internal-узел имеет одного или ни одного ребёнка — заменить и удалить internal.
    // В противном случае пересчитать кэши и вернуть сам inner.
    Node* collapseInternalIfNeeded(InternalNode* inner);

    Node* eraseRecursive(Node* node, int pos, int len);

    void getTextRangeRecursive(Node* node, int& offset, int& len, char* out, int& outPos) const;

    void buildKMPTable(const char* pattern, int patternLen, int* lps) const;

    int findSubstringRecursive(Node* node, const char* pattern, int patternLen, const int* lps, int& j, int& processed) const;

public:
    Tree(); // O(1) - Простая инициализация
    ~Tree(); // O(N) - Вызывает clear(), где N - количество узлов в дереве
    
    void clear(); // O(N) - Рекурсивно удаляет все узлы дерева
    bool isEmpty() const; // O(1) - Простая проверка указателя root
    
    // Построить дерево из текста
    void fromText(const char* text, int len); // O(N) - где N - длина текста. Рекурсивно делит текст пополам
    
    // Вытащить дерево в текст
    char* toText(); // O(N) - где N - общая длина текста. Выделяет память и рекурсивно собирает текст
    
    // Получить строку по номеру
    char* getLine(int lineNumber); // O(log M + L) - где M - количество узлов, L - максимальная длина листа
    
    // Получить количество строк в дереве
    int getTotalLineCount() const; // O(1) - Просто возвращает кэшированное значение из корня
    
    // Вычислить байтовое смещение для начала указанной строки внутри поддерева
    int getOffsetForLine(int lineIndex0Based) const; // O(log M + L) - где M - количество узлов, L - максимальная длина листа
    
    // возвращает новый буфер длиной len (или nullptr, если len==0).
    // Владелец вызывающий код должен вызвать delete[]
    char* getTextRange(int offset, int len) const; // O(log M + len) - где M - количество узлов

    int findSubstring(const char* pattern, int patternLen) const; // O(N) - где N - общая длина текста. Использует алгоритм Кнута-Морриса-Пратта
    
    // Возвращает номер строки (0-based), в которой начинается совпадение шаблона,
    // или -1 если не найдено.
    int findSubstringLine(const char* pattern, int patternLen) const; // O(N) - где N - общая длина текста
    
    // Вставка в дерево
    void insert(int pos, const char* data, int len); // O(log M + L) - где M - количество узлов, L - длина вставляемых данных

    // Удалить len байт, начиная с pos
    void erase(int pos, int len); // O(log M + L) - где M - количество узлов, L - длина удаляемых данных
    
    Node* getRoot() const; // O(1) - Простое получение указателя
    void setRoot(Node* newRoot); // O(1) - Простая установка указателя
};

#endif // TREE_H