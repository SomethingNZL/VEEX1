#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace veex {

// A single node in a KeyValues tree.
// A node is either a key/value pair OR a key/block pair (has children).
class KVNode {
public:
    std::string key;
    std::string value;                          // empty if this node is a block
    std::vector<std::shared_ptr<KVNode>> children;

    bool IsBlock() const { return !children.empty(); }

    // Find a direct child by key (case-insensitive)
    KVNode* GetChild(const std::string& key) const;

    // Get a value from a child by key, returning defaultValue if not found
    std::string Get(const std::string& key, const std::string& defaultValue = "") const;

    // Get all children with a given key
    std::vector<KVNode*> GetChildren(const std::string& key) const;
};

// Parses a VDF/KeyValues text file into a KVNode tree.
// Supports:
//   - Quoted keys and values: "key" "value"
//   - Nested blocks:          "key" { ... }
//   - Line comments:          // comment
class KeyValues {
public:
    // Parse from file. Returns nullptr on failure.
    static std::shared_ptr<KVNode> LoadFromFile(const std::string& path);

    // Parse from string. Returns nullptr on failure.
    static std::shared_ptr<KVNode> LoadFromString(const std::string& text);

private:
    struct Token {
        enum class Type { String, OpenBrace, CloseBrace, EndOfFile };
        Type type;
        std::string value;
    };

    std::string m_source;
    size_t m_pos = 0;
    int m_line = 1;

    explicit KeyValues(const std::string& source);

    Token NextToken();
    void SkipWhitespaceAndComments();
    std::string ReadQuotedString();
    std::string ReadUnquotedString();

    std::shared_ptr<KVNode> ParseNode();
    std::shared_ptr<KVNode> ParseBlock(const std::string& key);
};

} // namespace veex
