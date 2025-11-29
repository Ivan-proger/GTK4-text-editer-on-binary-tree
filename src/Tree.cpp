#include "Tree.h"
#include <cstring>

// ==========================================
// Реализация LeafNode
// ==========================================

LeafNode::LeafNode(const char* str, int len) {
    this->length = len;
    this->data = new char[len]; // NOSONAR
    this->lineCount = 0;
    
    // Инициализируем всю выделенную память нулями, чтобы избежать чтения "мусора".
    if (len > 0) {
        std::memset(this->data, 0, len);
    }

    // Копируем фактические данные, если str валиден.
    if (len > 0 && str) { 
        std::memcpy(this->data, str, len);
    }

    // Расчет теперь безопасен, т.к. this->data инициализирован.
    for (int i = 0; i < len; i++) {
        if (this->data[i] == '\n') {
            this->lineCount++;
        }
    }
}

LeafNode::~LeafNode() {
    delete[] data; // NOSONAR
}

NodeType LeafNode::getType() const { return NodeType::NODE_LEAF; }
int LeafNode::getLength() const { return length; }
int LeafNode::getLineCount() const { return lineCount; }

// ==========================================
// Реализация InternalNode
// ==========================================

InternalNode::InternalNode(Node* l, Node* r) {
    this->left = l;
    this->right = r;
    
    // Инициализируем нулями
    this->totalLength = 0;
    this->totalLineCount = 0;

    // Берем готовые данные из детей. Это O(1).
    if (l) {
        this->totalLength += l->getLength();
        this->totalLineCount += l->getLineCount();
    }
    if (r) {
        this->totalLength += r->getLength();
        this->totalLineCount += r->getLineCount();
    }
}

NodeType InternalNode::getType() const { return NodeType::NODE_INTERNAL; }
int InternalNode::getLength() const { return totalLength; }
int InternalNode::getLineCount() const { return totalLineCount; }


// ==========================================
// Реализация Tree
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
    
    // Используем getType, как ты хотел
    if (node->getType() == NodeType::NODE_INTERNAL) {
        auto inner = static_cast<InternalNode*>(node);
        clearRecursive(inner->left);
        clearRecursive(inner->right);
    }
    delete node; // NOSONAR // Виртуальный деструктор сработает корректно
}

bool Tree::isEmpty() const { return root == nullptr; }
Node* Tree::getRoot() const { return root; }

void Tree::setRoot(Node* newRoot) {
    if (root && root != newRoot) clear();
    root = newRoot;
}

// --- Построение (Logic Update) ---

Node* Tree::buildFromTextRecursive(const char* text, int len) {
    if (len <= 0) return nullptr;

    // УСЛОВИЕ ЛИСТА:
    // Если текст влезает в лимит размера, делаем лист.
    // Это гарантирует, что даже файл без \n будет разбит на куски.
    if (len <= MAX_LEAF_SIZE) {
        return new LeafNode(text, len); // NOSONAR
    } 

    // ПОИСК ТОЧКИ РАЗРЕЗА:
    // Пытаемся найти \n рядом с серединой, чтобы не резать слова.
    int half = len / 2;
    int splitIndex = -1;
    
    // Ищем \n в диапазоне +/- 256 байт от середины (или меньше, если файл мал)
    int searchRange = (len < 512) ? (len / 4) : 256;

    // Ищем вправо от середины
    for (int i = 0; i < searchRange; i++) {
        if (half + i < len && text[half + i] == '\n') {
            splitIndex = half + i + 1; // Режем ПОСЛЕ \n
            break;
        }
    }
    
    // Если не нашли, ищем влево
    if (splitIndex == -1) {
        for (int i = 0; i < searchRange; i++) {
            if (half - i > 0 && text[half - i] == '\n') {
                splitIndex = half - i + 1;
                break;
            }
        }
    }

    // ФОЛЛБЭК (Fallback):
    // Если \n нет (minified файл) или он далеко — режем жестко пополам.
    if (splitIndex == -1) {
        splitIndex = half;
    }

    Node* left = buildFromTextRecursive(text, splitIndex);
    Node* right = buildFromTextRecursive(text + splitIndex, len - splitIndex);

    return new InternalNode(left, right); // NOSONAR
}

void Tree::fromText(const char* text, int len) {
    clear();
    if (!text || len <= 0) return;
    root = buildFromTextRecursive(text, len);
}

// --- Экспорт в текст ---

void Tree::collectTextRecursive(Node* node, char* buffer, int& pos) {
    if (!node) return;
    
    if (node->getType() == NodeType::NODE_LEAF) {
        auto leaf = static_cast<LeafNode*>(node);
        // memcpy быстрее цикла
        if (leaf->length > 0) {
            std::memcpy(buffer + pos, leaf->data, leaf->length);
            pos += leaf->length;
        }
    } else {
        auto inner = static_cast<InternalNode*>(node);
        collectTextRecursive(inner->left, buffer, pos);
        collectTextRecursive(inner->right, buffer, pos);
    }
}

char* Tree::toText() {
    if (!root) {
        auto empty = new char[1]; // NOSONAR
        empty[0] = '\0';
        return empty;
    }
    
    // ТЕПЕРЬ МЫ ЗНАЕМ ДЛИНУ ЗА O(1)!
    // Не нужно запускать calculateLengthRecursive
    int totalLen = root->getLength();
    
    auto buffer = new char[totalLen + 1]; // NOSONAR
    int pos = 0;
    collectTextRecursive(root, buffer, pos);
    buffer[pos] = '\0';
    return buffer;
}

// --- Получение строки (Get Line) ---

LeafNode* Tree::findLeafByLineRecursive(Node* node, int& localLineIndex) {
    if (!node) return nullptr;

    if (node->getType() == NodeType::NODE_LEAF) {
        return static_cast<LeafNode*>(node);
    }

    auto inner = static_cast<InternalNode*>(node);
    
    // Ключевой момент оптимизации:
    // Мы спрашиваем у левого ребенка, сколько в нем строк. Это O(1) операция.
    int leftLines = 0;
    if (inner->left) {
        leftLines = inner->left->getLineCount();
    }

    if (localLineIndex < leftLines) {
        // Искомая строка слева
        return findLeafByLineRecursive(inner->left, localLineIndex);
    } else {
        // Искомая строка справа. Корректируем индекс.
        localLineIndex -= leftLines;
        return findLeafByLineRecursive(inner->right, localLineIndex);
    }
}

char* Tree::getLine(int lineNumber) {
    if (!root || lineNumber < 0) return nullptr;
    
    // Проверка: а есть ли такая строка вообще? (Опционально, но полезно)
    if (lineNumber > root->getLineCount()) return nullptr;

    int localIndex = lineNumber;
    // Этот метод спустится по дереву за O(log N) и вернет Лист.
    // localIndex станет номером строки ВНУТРИ этого листа.
    auto leaf = findLeafByLineRecursive(root, localIndex);

    if (!leaf) return nullptr;

    // Дальше логика поиска внутри листа (почти как у тебя было)
    int currentLine = 0;
    int startPos = 0;
    int endPos = -1;
    bool foundStart = false;

    // Если localIndex == 0, значит строка начинается с начала листа
    if (localIndex == 0) foundStart = true;


    for (int i = 0; i < leaf->length; i++) {
        // Поиск конца строки (выполняется, если начало уже найдено)
        if (foundStart) { // Уровень 2
            if (leaf->data[i] == '\n') { // Уровень 3
                endPos = i;
                break;
            }
            continue; // Если не нашли конец, продолжаем сканирование
        }
        
        // Поиск начала строки (выполняется, если !foundStart)
        
        // Если это не перенос строки, пропускаем (Level 2)
        if (leaf->data[i] != '\n') {
            continue;
        }

        // Это перенос строки, увеличиваем счетчик
        currentLine++;
        
        // Проверяем, достигли ли мы нужной строки (Level 2)
        if (currentLine == localIndex) {
            startPos = i + 1; // Начало сразу после \n
            foundStart = true;
        }
        
        // После нахождения начала, продолжаем итерацию, чтобы следующий символ
        // был обработан блоком 'if (foundStart)' для поиска конца.
    }

    // Если дошли до конца листа, а \n не встретили — значит строка идет до конца блока
    if (foundStart && endPos == -1) {
        endPos = leaf->length;
    }

    if (!foundStart) return nullptr; // Что-то пошло не так

    int lineLen = endPos - startPos;
    // Защита от отрицательной длины
    if (lineLen < 0) lineLen = 0; 

    auto result = new char[lineLen + 1]; // NOSONAR
    if (lineLen > 0) {
        std::memcpy(result, leaf->data + startPos, lineLen);
    }
    result[lineLen] = '\0';

    return result;
}