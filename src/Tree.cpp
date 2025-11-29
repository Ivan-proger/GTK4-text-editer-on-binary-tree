#include "Tree.h"
#include <cstring>

// ==========================================
// Реализация Узлов
// ==========================================

LeafNode::LeafNode(const char* str, int len) {
    this->length = len;
    this->data = new char[len]; // NOSONAR
    if (len > 0 && str) std::memcpy(this->data, str, len);
}

LeafNode::~LeafNode() {
    if (data) delete[] data; // NOSONAR
}

NodeType LeafNode::getType() const { return NodeType::NODE_LEAF; }

int countLines(Node* node) {
    if (!node) return 0;
        
    if (node->getType() == NodeType::NODE_INTERNAL) {
        // У внутреннего узла это значение уже посчитано и закэшировано
        return ((InternalNode*)node)->subtreeCount;
    } else {
        // У листа считаем \n "на лету"
        auto leaf = (LeafNode*)node;
        int count = 0;
        for (int i = 0; i < leaf->length; i++) {
            if (leaf->data[i] == '\n') count++;
        }
        // Если текст не заканчивается на \n, но данные есть, это считается за строку
        // Но для точной математики разбиения часто проще считать именно символы \n.
        // Давай договоримся: subtreeCount хранит количество символов '\n'.
        return count; 
    }
}

InternalNode::InternalNode(Node* l, Node* r) {
    this->left = l;
    this->right = r;
    this->subtreeCount = 0; // Теперь это счетчик строк!
    
    // Суммируем строки левого и правого ребенка
    if (l) this->subtreeCount += countLines(l);
    if (r) this->subtreeCount += countLines(r);
}

NodeType InternalNode::getType() const { return NodeType::NODE_INTERNAL; }

// ==========================================
// Реализация класса Tree (Логика в памяти)
// ==========================================

Tree::Tree() : root(nullptr) {}

Tree::~Tree() {
    clear();
}

void Tree::clear() {
    clearRecursive(root);
    root = nullptr;
}

void Tree::clearRecursive(Node* node) {
    if (!node) return;
    if (node->getType() == NodeType::NODE_INTERNAL) {
        auto inner = static_cast<InternalNode*>(node);
        clearRecursive(inner->left);
        clearRecursive(inner->right);
    }
    delete node;// NOSONAR // Виртуальный деструктор сработает правильно
}

bool Tree::isEmpty() const {
    return root == nullptr;
}

Node* Tree::getRoot() const {
    return root;
}

void Tree::setRoot(Node* newRoot) {
    // Если дерево не пустое, сначала чистим старое, чтобы не было утечки
    // Защита от ситуации: setRoot(root) — не будем очищать и удалять сам newRoot
    if (root && root != newRoot) clear();
    root = newRoot;
}

// --- Логика построения из текста ---

Node* Tree::buildFromTextRecursive(const char* text, int len) {
if (len <= 0) return nullptr;

    // 1. Считаем количество переносов строк в текущем куске
    int newlines = 0;
    for(int i = 0; i < len; i++) if(text[i] == '\n') newlines++;

    // 2. Если строк мало (например, < 2), создаем Лист
    // Это базовый случай рекурсии
    if (newlines < 2) {
        return new LeafNode(text, len); // NOSONAR
    }

    // 3. Ищем середину по СТРОКАМ (а не по байтам)
    int targetNewline = newlines / 2;
    int splitIndex = 0;
    int currentNewlineCount = 0;

    for (int i = 0; i < len; i++) {
        if (text[i] == '\n') {
            currentNewlineCount++;
            if (currentNewlineCount == targetNewline) {
                splitIndex = i + 1; // Режем сразу после \n
                break;
            }
        }
    }

    // Рекурсивно создаем детей
    Node* left = buildFromTextRecursive(text, splitIndex);
    Node* right = buildFromTextRecursive(text + splitIndex, len - splitIndex);

    return new InternalNode(left, right); // NOSONAR
}

void Tree::fromText(const char* text, int len) {
    clear();
    // Используем переданную длину len, а не ищем \0
    if (!text || len <= 0) return;
    root = buildFromTextRecursive(text, len);
}

// --- Логика экспорта в текст ---

int Tree::calculateLengthRecursive(Node* node) {
    if (!node) return 0;
    if (node->getType() == NodeType::NODE_LEAF) {
        return ((LeafNode*)node)->length;
    } else {
        auto inner = static_cast<InternalNode*>(node);
        return calculateLengthRecursive(inner->left) + calculateLengthRecursive(inner->right);
    }
}

void Tree::collectTextRecursive(Node* node, char* buffer, int& pos) {
    if (!node) return;
    if (node->getType() == NodeType::NODE_LEAF) {
        auto leaf = (LeafNode*)node;
        for (int i = 0; i < leaf->length; i++) {
            buffer[pos++] = leaf->data[i];
        }
    } else {
        auto inner = static_cast<InternalNode*>(node);
        collectTextRecursive(inner->left, buffer, pos);
        collectTextRecursive(inner->right, buffer, pos);
    }
}

char* Tree::toText() {
    if (!root) {
        // Возвращаем всегда валидный буфер (пустая строка), чтобы у вызывающего не было необходимости проверять nullptr
        auto empty = new char[1]; // NOSONAR
        empty[0] = '\0';
        return empty;
    }
    int totalLen = calculateLengthRecursive(root);
    
    auto buffer = new char[totalLen + 1]; // NOSONAR
    int pos = 0;
    collectTextRecursive(root, buffer, pos);
    buffer[pos] = '\0';
    return buffer;
}

// Функция ищет нужный лист и индекс строки внутри него.
// Возвращает указатель на лист и изменяет localLineIndex
LeafNode* findLeafRecursive(Node* node, int& lineIndex) {
    if (!node) return nullptr;

    if (node->getType() == NodeType::NODE_LEAF) {
        return (LeafNode*)node;
    }

    auto inner = (InternalNode*)node; // NOSONAR
    int leftLines = countLines(inner->left);

    if (lineIndex < leftLines) {
        // Идем влево, индекс строки не меняется
        return findLeafRecursive(inner->left, lineIndex);
    } else {
        // Идем вправо, уменьшаем индекс на количество строк слева
        lineIndex -= leftLines;
        return findLeafRecursive(inner->right, lineIndex);
    }
}

char* Tree::getLine(int lineNumber) {
    if (!root || lineNumber < 0) return nullptr;

    int localIndex = lineNumber;
    // 1. Быстрый спуск по дереву за O(log N)
    auto leaf = findLeafRecursive(root, localIndex);

    if (!leaf) return nullptr;

    // 2. Теперь мы внутри листа. Нам нужно найти localIndex-ную строку
    // localIndex = 0 означает первую строку ВНУТРИ этого листа
    
    int currentLine = 0;
    int startPos = 0;
    int endPos = -1;

    // Пробегаем по листу, ищем начало и конец
    for (int i = 0; i < leaf->length; i++) {
        if (currentLine == localIndex) {
            // Мы нашли нужную строку
            // Ищем её конец (либо \n, либо конец данных)
            int j = i;
            while (j < leaf->length && leaf->data[j] != '\n') {
                j++;
            }
            endPos = j; // j указывает на \n или на позицию за пределами массива
            startPos = i;
            break;
        }

        if (leaf->data[i] == '\n') {
            currentLine++;
        }
    }

    // Если строку не нашли (например, запросили строку 5, а в листе их всего 2)
    if (endPos == -1) return nullptr;

    // 3. Выделяем память и копируем
    int lineLen = endPos - startPos;
    auto result = new char[lineLen + 1]; // NOSONAR
    
    if (lineLen > 0) {
        std::memcpy(result, leaf->data + startPos, lineLen);
    }
    result[lineLen] = '\0'; // Обязательно добавляем терминальный ноль

    return result;
}