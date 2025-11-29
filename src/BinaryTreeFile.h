#ifndef BINARY_TREE_FILE_H
#define BINARY_TREE_FILE_H

#include "Tree.h" // Нужен для доступа к структурам Node и классу Tree
#include <fstream>
#include <cstdint>

class BinaryTreeFile : public std::fstream {
private:
    // Имя файла, чтобы можно было усечь/переоткрыть при сохранении
    std::string m_filename; 

    // Рекурсивные методы I/O, работающие с узлами (Node*)
    std::int64_t  writeNodeRecursive(Node* node);

    Node* readLeafNodeAt(std::int64_t offset, std::int64_t fileSize);
    Node* readInternalNodeAt(std::int64_t offset, std::int64_t fileSize);
    Node* readNodeRecursive(std::int64_t offset, std::int64_t fileSize);

    // Вспомогательные: чтение/запись в little-endian фиксированных типов
    void write_le_int32(std::int32_t v);
    void write_le_int64(std::int64_t v);
    void write_le_uint32(std::uint32_t v);

    std::int32_t read_le_int32();
    std::int64_t read_le_int64();
    std::uint32_t read_le_uint32();

public:
    BinaryTreeFile();
    ~BinaryTreeFile() override;

    bool openFile(const char* filename);
    
    // Основные операции сериализации/десериализации
    void saveTree(const Tree& tree);
    void loadTree(Tree& tree);
};

#endif // BINARY_TREE_FILE_H