#ifndef TREE_H
#define TREE_H

// Настройка размера листа. 4KB - золотая середина (размер страницы памяти)
const int MAX_LEAF_SIZE = 4096; 

enum class NodeType : char {
    NODE_INTERNAL = 0,
    NODE_LEAF = 1
};

struct Node {
    // Оставляем getType, как ты просил
    virtual NodeType getType() const = 0;
    
    // Новые виртуальные методы для быстрого доступа к статистике
    virtual int getLength() const = 0;     // Вес в байтах
    virtual int getLineCount() const = 0;  // Вес в строках (\n)
    
    virtual ~Node() = default;
};

struct LeafNode : public Node {
    int length;
    int lineCount; // Кэшированное значение
    char* data;

    LeafNode(const char* str, int len);
    ~LeafNode() override;
    
    NodeType getType() const override;
    int getLength() const override;
    int getLineCount() const override;
};

struct InternalNode : public Node {
    Node* left;
    Node* right;

    // Кэшированные суммы детей
    int totalLength;
    int totalLineCount;

    InternalNode(Node* l, Node* r);
    ~InternalNode() override = default;
    
    NodeType getType() const override;
    int getLength() const override;
    int getLineCount() const override;
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

public:
    Tree();
    ~Tree();

    void clear();
    bool isEmpty() const;

    void fromText(const char* text, int len);
    char* toText();
    
    char* getLine(int lineNumber);

    Node* getRoot() const;
    void setRoot(Node* newRoot);
};

#endif // TREE_H