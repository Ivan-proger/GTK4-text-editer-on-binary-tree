#ifndef TREE_H
#define TREE_H

// --- Структуры данных (Узлы) ---

enum NodeType : char {
    NODE_INTERNAL = 0,
    NODE_LEAF = 1
};

struct Node {
    virtual NodeType getType() const = 0;
    virtual ~Node() {}
};

struct LeafNode : public Node {
    int length;
    char* data;

    LeafNode(const char* str, int len);
    ~LeafNode();
    NodeType getType() const override;
};

struct InternalNode : public Node {
    Node* left;
    Node* right;
    int subtreeCount;

    InternalNode(Node* l, Node* r);
    ~InternalNode() {}
    NodeType getType() const override;
};

// --- Класс Дерева (Логика в памяти) ---

class Tree {
private:
    Node* root;

    // Вспомогательные рекурсивные методы

    void clearRecursive(Node* node);

    Node* buildFromTextRecursive(const char* text, int len);

    int calculateLengthRecursive(Node* node);

    void collectTextRecursive(Node* node, char* buffer, int& pos);



public:
    Tree();
    ~Tree();

    void clear();
    bool isEmpty() const;

    // Методы для работы с данными
    void fromText(const char* text);
    char* toText(); // Возвращает указатель, который нужно удалить через delete[]

    // Геттеры/Сеттеры для сериализатора
    Node* getRoot() const;
    void setRoot(Node* newRoot);
};

#endif // TREE_H