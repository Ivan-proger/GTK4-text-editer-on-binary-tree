#include "BinaryTreeFile.h"
#include <stdexcept>
#include <iostream>
#include <cstring>

// ==========================================
// Реализация класса BinaryTreeFile
// ==========================================

namespace {
    constexpr char FILE_MAGIC[4] = {'T','R','E','E'};
    constexpr std::uint32_t FILE_VERSION = 1;
    constexpr std::int64_t OFFSET_NONE = -1;
}


BinaryTreeFile::BinaryTreeFile() : std::fstream() {}

BinaryTreeFile::~BinaryTreeFile() {
    if (is_open()) close();
}

bool BinaryTreeFile::openFile(const char* filename) {
    open(filename, std::ios::binary | std::ios::in | std::ios::out);
    if (!is_open()) {
        // Создаем, если не существует
        open(filename, std::ios::binary | std::ios::out);
        close();
        open(filename, std::ios::binary | std::ios::in | std::ios::out);
    }
    return is_open();
}

// --- Сохранение ---

std::int64_t BinaryTreeFile::writeNodeRecursive(Node* node) {
    if (!node) return OFFSET_NONE;

    std::int64_t leftOff = OFFSET_NONE;
    std::int64_t rightOff = OFFSET_NONE;

    if (node->getType() == NODE_INTERNAL) {
        InternalNode* inner = static_cast<InternalNode*>(node);
        leftOff = writeNodeRecursive(inner->left);
        rightOff = writeNodeRecursive(inner->right);
    }

    seekp(0, std::ios::end);
    std::int64_t currentPos = static_cast<std::int64_t>(tellp());

    char type = static_cast<char>(node->getType());
    write(&type, sizeof(char));
    if (!good()) throw std::runtime_error("I/O error writing node type");

    if (node->getType() == NODE_LEAF) {
        LeafNode* leaf = static_cast<LeafNode*>(node);
        write_le_int32(static_cast<std::int32_t>(leaf->length));
        if (leaf->length > 0) {
            write(leaf->data, leaf->length);
            if (!good()) throw std::runtime_error("I/O error writing leaf data");
        }
    } else {
        InternalNode* inner = static_cast<InternalNode*>(node);
        write_le_int64(leftOff);
        write_le_int64(rightOff);
        write_le_int32(static_cast<std::int32_t>(inner->subtreeCount));
    }
    return currentPos;
}


void BinaryTreeFile::saveTree(const Tree& tree) {
    if (!is_open()) return;

    // Запишем заголовок: magic(4) + version(4) + rootOffset(8) (всего 16 байт)
    seekp(0, std::ios::beg);
    write(FILE_MAGIC, 4);
    if (!good()) throw std::runtime_error("I/O error writing magic");
    write_le_uint32(FILE_VERSION);
    // временно пишем OFFSET_NONE в качестве плэйсхолдера
    write_le_int64(OFFSET_NONE);

    // Пишем узлы (post-order) — функция вернёт смещение root
    std::int64_t rootOffset = writeNodeRecursive(tree.getRoot());

    // Записываем реальный rootOffset в заголовок
    seekp(4 + 4, std::ios::beg); // magic(4) + version(4)
    write_le_int64(rootOffset);
    flush();
}


// --- Загрузка ---

Node* BinaryTreeFile::readNodeRecursive(std::int64_t offset, std::int64_t fileSize) {
    if (offset == OFFSET_NONE) return nullptr;
    if (offset < 0 || offset >= fileSize) {
        throw std::runtime_error("Invalid node offset (out of file bounds)");
    }

    seekg(offset, std::ios::beg);
    char type;
    read(&type, sizeof(char));
    if (gcount() != 1 || !good()) throw std::runtime_error("I/O error reading node type");

    if (type == static_cast<char>(NODE_LEAF)) {
        // Проверим, что есть место для len
        std::int64_t minNeeded = offset + 1 + static_cast<std::int64_t>(sizeof(std::int32_t));
        if (minNeeded > fileSize) throw std::runtime_error("Corrupt file: not enough bytes for leaf length");

        std::int32_t len = read_le_int32();
        if (len < 0) throw std::runtime_error("Corrupt file: negative leaf length");
        // Проверяем доступность данных
        std::int64_t needed = offset + 1 + static_cast<std::int64_t>(sizeof(std::int32_t)) + static_cast<std::int64_t>(len);
        if (needed > fileSize) throw std::runtime_error("Corrupt file: leaf data exceeds file size");

        char* buf = new char[len];
        if (len > 0) {
            read(buf, len);
            if (gcount() != len || !good()) {
                delete[] buf;
                throw std::runtime_error("I/O error reading leaf data");
            }
        }
        Node* node = new LeafNode(buf, len);
        delete[] buf;
        return node;
    } else if (type == static_cast<char>(NODE_INTERNAL)) {
        // Проверим, что есть место для двух offset (int64) и count (int32)
        std::int64_t headerNeeded = offset + 1 + static_cast<std::int64_t>(sizeof(std::int64_t)) * 2 + static_cast<std::int64_t>(sizeof(std::int32_t));
        if (headerNeeded > fileSize) throw std::runtime_error("Corrupt file: not enough bytes for internal header");

        std::int64_t lOff = read_le_int64();
        std::int64_t rOff = read_le_int64();
        std::int32_t count = read_le_int32();

        // Дополнительная валидация оффсетов
        if ( (lOff != OFFSET_NONE && (lOff < 0 || lOff >= fileSize)) ||
             (rOff != OFFSET_NONE && (rOff < 0 || rOff >= fileSize)) ) {
            throw std::runtime_error("Corrupt file: child offset out of bounds");
        }

        Node* l = readNodeRecursive(lOff, fileSize);
        Node* r = readNodeRecursive(rOff, fileSize);
        
        InternalNode* node = new InternalNode(l, r);
        node->subtreeCount = count;
        return node;
    } else {
        throw std::runtime_error("Unknown node type in file");
    }
}


void BinaryTreeFile::loadTree(Tree& tree) {
    if (!is_open()) return;

    tree.clear();

    seekg(0, std::ios::end);
    std::int64_t fileSize = static_cast<std::int64_t>(tellg());
    if (fileSize <= 0) return;

    seekg(0, std::ios::beg);
    char magic[4];
    read(magic, 4);
    if (gcount() != 4 || !good()) throw std::runtime_error("I/O error reading magic");
    if (std::memcmp(magic, FILE_MAGIC, 4) != 0) throw std::runtime_error("Bad file magic - not a tree file");

    std::uint32_t version = read_le_uint32();
    if (version != FILE_VERSION) throw std::runtime_error("Unsupported file version");

    std::int64_t rootOffset = read_le_int64();
    if (rootOffset == OFFSET_NONE) {
        tree.setRoot(nullptr);
        return;
    }
    if (rootOffset < 0 || rootOffset >= fileSize) throw std::runtime_error("Invalid root offset in header");

    Node* newRoot = readNodeRecursive(rootOffset, fileSize);
    tree.setRoot(newRoot);
}


void BinaryTreeFile::write_le_uint32(std::uint32_t v) {
    unsigned char b[4];
    b[0] = static_cast<unsigned char>((v) & 0xFF);
    b[1] = static_cast<unsigned char>((v >> 8) & 0xFF);
    b[2] = static_cast<unsigned char>((v >> 16) & 0xFF);
    b[3] = static_cast<unsigned char>((v >> 24) & 0xFF);
    write(reinterpret_cast<char*>(b), 4);
    if (!good()) throw std::runtime_error("I/O error writing uint32");
}

void BinaryTreeFile::write_le_int32(std::int32_t v) {
    write_le_uint32(static_cast<std::uint32_t>(v));
}

void BinaryTreeFile::write_le_int64(std::int64_t v) {
    unsigned char b[8];
    std::uint64_t uv = static_cast<std::uint64_t>(v);
    for (int i = 0; i < 8; ++i) b[i] = static_cast<unsigned char>((uv >> (8 * i)) & 0xFF);
    write(reinterpret_cast<char*>(b), 8);
    if (!good()) throw std::runtime_error("I/O error writing int64");
}

std::uint32_t BinaryTreeFile::read_le_uint32() {
    unsigned char b[4];
    read(reinterpret_cast<char*>(b), 4);
    if (gcount() != 4 || !good()) throw std::runtime_error("I/O error reading uint32");
    std::uint32_t v = (static_cast<std::uint32_t>(b[0]) |
                       (static_cast<std::uint32_t>(b[1]) << 8) |
                       (static_cast<std::uint32_t>(b[2]) << 16) |
                       (static_cast<std::uint32_t>(b[3]) << 24));
    return v;
}

std::int32_t BinaryTreeFile::read_le_int32() {
    return static_cast<std::int32_t>(read_le_uint32());
}

std::int64_t BinaryTreeFile::read_le_int64() {
    unsigned char b[8];
    read(reinterpret_cast<char*>(b), 8);
    if (gcount() != 8 || !good()) throw std::runtime_error("I/O error reading int64");
    std::uint64_t uv = 0;
    for (int i = 0; i < 8; ++i) uv |= (static_cast<std::uint64_t>(b[i]) << (8 * i));
    return static_cast<std::int64_t>(uv);
}
