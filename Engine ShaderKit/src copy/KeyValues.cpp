#include "veex/KeyValues.h"
#include "veex/Logger.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace veex {

// ─── KVNode ──────────────────────────────────────────────────────────────────

static std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return s;
}

KVNode* KVNode::GetChild(const std::string& key) const {
    std::string lower = ToLower(key);
    for (const auto& child : children) {
        if (ToLower(child->key) == lower)
            return child.get();
    }
    return nullptr;
}

std::string KVNode::Get(const std::string& key, const std::string& defaultValue) const {
    KVNode* child = GetChild(key);
    if (!child)
        return defaultValue;
    return child->value;
}

std::vector<KVNode*> KVNode::GetChildren(const std::string& key) const {
    std::string lower = ToLower(key);
    std::vector<KVNode*> result;
    for (const auto& child : children) {
        if (ToLower(child->key) == lower)
            result.push_back(child.get());
    }
    return result;
}

// ─── KeyValues parser ─────────────────────────────────────────────────────────

KeyValues::KeyValues(const std::string& source)
    : m_source(source), m_pos(0), m_line(1)
{
}

std::shared_ptr<KVNode> KeyValues::LoadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        Logger::Error("KeyValues: failed to open file: " + path);
        return nullptr;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return LoadFromString(ss.str());
}

std::shared_ptr<KVNode> KeyValues::LoadFromString(const std::string& text) {
    KeyValues parser(text);

    // A VDF file is a sequence of top-level nodes; wrap them in a root node.
    auto root = std::make_shared<KVNode>();
    root->key = "__root__";

    while (true) {
        auto node = parser.ParseNode();
        if (!node) break;
        root->children.push_back(node);
    }

    return root;
}

void KeyValues::SkipWhitespaceAndComments() {
    while (m_pos < m_source.size()) {
        char c = m_source[m_pos];

        if (c == '\n') { ++m_line; ++m_pos; continue; }
        if (std::isspace((unsigned char)c)) { ++m_pos; continue; }

        // Line comment
        if (c == '/' && m_pos + 1 < m_source.size() && m_source[m_pos + 1] == '/') {
            while (m_pos < m_source.size() && m_source[m_pos] != '\n')
                ++m_pos;
            continue;
        }

        break;
    }
}

std::string KeyValues::ReadQuotedString() {
    ++m_pos; // skip opening quote
    std::string result;
    while (m_pos < m_source.size()) {
        char c = m_source[m_pos++];
        if (c == '"') break;
        if (c == '\\' && m_pos < m_source.size()) {
            char esc = m_source[m_pos++];
            switch (esc) {
                case 'n':  result += '\n'; break;
                case 't':  result += '\t'; break;
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                default:   result += esc;  break;
            }
            continue;
        }
        if (c == '\n') ++m_line;
        result += c;
    }
    return result;
}

std::string KeyValues::ReadUnquotedString() {
    std::string result;
    while (m_pos < m_source.size()) {
        char c = m_source[m_pos];
        if (std::isspace((unsigned char)c) || c == '{' || c == '}' || c == '"')
            break;
        result += c;
        ++m_pos;
    }
    return result;
}

KeyValues::Token KeyValues::NextToken() {
    SkipWhitespaceAndComments();

    if (m_pos >= m_source.size())
        return { Token::Type::EndOfFile, "" };

    char c = m_source[m_pos];

    if (c == '{') { ++m_pos; return { Token::Type::OpenBrace, "" }; }
    if (c == '}') { ++m_pos; return { Token::Type::CloseBrace, "" }; }
    if (c == '"') return { Token::Type::String, ReadQuotedString() };

    return { Token::Type::String, ReadUnquotedString() };
}

std::shared_ptr<KVNode> KeyValues::ParseNode() {
    Token keyToken = NextToken();

    if (keyToken.type == Token::Type::EndOfFile ||
        keyToken.type == Token::Type::CloseBrace)
        return nullptr;

    if (keyToken.type != Token::Type::String) {
        Logger::Error("KeyValues: unexpected token on line " + std::to_string(m_line));
        return nullptr;
    }

    std::string key = keyToken.value;

    // Peek at the next token to decide value vs block
    size_t savedPos = m_pos;
    int savedLine = m_line;
    Token next = NextToken();

    if (next.type == Token::Type::OpenBrace) {
        return ParseBlock(key);
    } else if (next.type == Token::Type::String) {
        auto node = std::make_shared<KVNode>();
        node->key = key;
        node->value = next.value;
        return node;
    } else {
        // Bare key with no value or block — treat as empty value
        m_pos = savedPos;
        m_line = savedLine;
        auto node = std::make_shared<KVNode>();
        node->key = key;
        node->value = "";
        return node;
    }
}

std::shared_ptr<KVNode> KeyValues::ParseBlock(const std::string& key) {
    auto node = std::make_shared<KVNode>();
    node->key = key;

    while (true) {
        size_t savedPos = m_pos;
        int savedLine = m_line;
        Token peek = NextToken();

        if (peek.type == Token::Type::EndOfFile) {
            Logger::Warn("KeyValues: unexpected EOF inside block '" + key + "'");
            break;
        }
        if (peek.type == Token::Type::CloseBrace)
            break;

        // Put the token back by restoring position
        m_pos = savedPos;
        m_line = savedLine;

        auto child = ParseNode();
        if (child)
            node->children.push_back(child);
    }

    return node;
}

} // namespace veex
