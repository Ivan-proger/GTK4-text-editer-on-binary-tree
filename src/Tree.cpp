#include "Tree.h"
#include <cstring>

// ==========================================
// Реализация Узлов
// ==========================================

LeafNode::LeafNode(const char* str, int len) {
    this->length = len;
    this->data = new char[len];
    if (len > 0 && str) std::memcpy(this->data, str, len);
}

LeafNode::~LeafNode() {
    if (data) delete[] data;
}

NodeType LeafNode::getType() const { return NODE_LEAF; }

InternalNode::InternalNode(Node* l, Node* r) {
    this->left = l;
    this->right = r;
    this->subtreeCount = 1;
    if (l) {
        if (l->getType() == NODE_INTERNAL) this->subtreeCount += ((InternalNode*)l)->subtreeCount;
        else this->subtreeCount++;
    }
    if (r) {
        if (r->getType() == NODE_INTERNAL) this->subtreeCount += ((InternalNode*)r)->subtreeCount;
        else this->subtreeCount++;
    }
}

NodeType InternalNode::getType() const { return NODE_INTERNAL; }

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
    if (node->getType() == NODE_INTERNAL) {
        InternalNode* inner = static_cast<InternalNode*>(node);
        clearRecursive(inner->left);
        clearRecursive(inner->right);
    }
    delete node; // Виртуальный деструктор сработает правильно
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
    
    // Для теста разбиваем мелко (по 4 символа), чтобы дерево было ветвистым
    const int MAX_CHUNK = 4; 

    if (len <= MAX_CHUNK) {
        return new LeafNode(text, len);
    }

    int mid = len / 2;
    Node* left = buildFromTextRecursive(text, mid);
    Node* right = buildFromTextRecursive(text + mid, len - mid);
    return new InternalNode(left, right);
}

void Tree::fromText(const char* text) {
    clear();
    if (!text) return;
    int len = 0;
    while(text[len] != '\0') len++;
    root = buildFromTextRecursive(text, len);
}

// --- Логика экспорта в текст ---

int Tree::calculateLengthRecursive(Node* node) {
    if (!node) return 0;
    if (node->getType() == NODE_LEAF) {
        return ((LeafNode*)node)->length;
    } else {
        InternalNode* inner = static_cast<InternalNode*>(node);
        return calculateLengthRecursive(inner->left) + calculateLengthRecursive(inner->right);
    }
}

void Tree::collectTextRecursive(Node* node, char* buffer, int& pos) {
    if (!node) return;
    if (node->getType() == NODE_LEAF) {
        LeafNode* leaf = (LeafNode*)node;
        for (int i = 0; i < leaf->length; i++) {
            buffer[pos++] = leaf->data[i];
        }
    } else {
        InternalNode* inner = static_cast<InternalNode*>(node);
        collectTextRecursive(inner->left, buffer, pos);
        collectTextRecursive(inner->right, buffer, pos);
    }
}

char* Tree::toText() {
    if (!root) {
        // Возвращаем всегда валидный буфер (пустая строка), чтобы у вызывающего не было необходимости проверять nullptr
        char* empty = new char[1];
        empty[0] = '\0';
        return empty;
    }
    int totalLen = calculateLengthRecursive(root);
    
    char* buffer = new char[totalLen + 1];
    int pos = 0;
    collectTextRecursive(root, buffer, pos);
    buffer[pos] = '\0';
    return buffer;
}