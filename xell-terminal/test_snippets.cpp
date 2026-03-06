#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <unordered_map>

// Inline the snippet engine for testing
struct SnippetDef
{
    std::string prefix;
    std::string description;
    std::vector<std::string> bodyLines;
};

static void skipWhitespace(const std::string &s, size_t &pos)
{
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r'))
        pos++;
}

static std::string readString(const std::string &s, size_t &pos)
{
    skipWhitespace(s, pos);
    if (pos >= s.size() || s[pos] != '"')
        return "";
    pos++;

    std::string result;
    while (pos < s.size())
    {
        if (s[pos] == '\\' && pos + 1 < s.size())
        {
            char next = s[pos + 1];
            if (next == '"')
                result += '"';
            else if (next == '\\')
                result += '\\';
            else if (next == 'n')
                result += '\n';
            else if (next == 't')
                result += '\t';
            else
            {
                result += '\\';
                result += next;
            }
            pos += 2;
        }
        else if (s[pos] == '"')
        {
            pos++;
            return result;
        }
        else
        {
            result += s[pos];
            pos++;
        }
    }
    return result;
}

static void skipValue(const std::string &s, size_t &pos)
{
    skipWhitespace(s, pos);
    if (pos >= s.size())
        return;

    if (s[pos] == '"')
    {
        readString(s, pos);
    }
    else if (s[pos] == '{')
    {
        int depth = 1;
        pos++;
        while (pos < s.size() && depth > 0)
        {
            if (s[pos] == '{')
                depth++;
            else if (s[pos] == '}')
                depth--;
            else if (s[pos] == '"')
            {
                readString(s, pos);
                continue;
            }
            pos++;
        }
    }
    else if (s[pos] == '[')
    {
        int depth = 1;
        pos++;
        while (pos < s.size() && depth > 0)
        {
            if (s[pos] == '[')
                depth++;
            else if (s[pos] == ']')
                depth--;
            else if (s[pos] == '"')
            {
                readString(s, pos);
                continue;
            }
            pos++;
        }
    }
    else
    {
        while (pos < s.size() && s[pos] != ',' && s[pos] != '}' && s[pos] != ']')
            pos++;
    }
}

static bool parseSnippetObject(const std::string &json, size_t &pos, SnippetDef &def)
{
    skipWhitespace(json, pos);
    if (pos >= json.size() || json[pos] != '{')
        return false;
    pos++;

    while (pos < json.size())
    {
        skipWhitespace(json, pos);
        if (pos >= json.size())
            return false;
        if (json[pos] == '}')
        {
            pos++;
            return true;
        }
        if (json[pos] == ',')
        {
            pos++;
            continue;
        }

        std::string key = readString(json, pos);
        skipWhitespace(json, pos);
        if (pos >= json.size() || json[pos] != ':')
            return false;
        pos++;
        skipWhitespace(json, pos);

        if (key == "prefix")
        {
            def.prefix = readString(json, pos);
        }
        else if (key == "description")
        {
            def.description = readString(json, pos);
        }
        else if (key == "body")
        {
            if (pos < json.size() && json[pos] == '[')
            {
                pos++;
                while (pos < json.size())
                {
                    skipWhitespace(json, pos);
                    if (pos >= json.size() || json[pos] == ']')
                    {
                        pos++;
                        break;
                    }
                    if (json[pos] == ',')
                    {
                        pos++;
                        continue;
                    }
                    std::string line = readString(json, pos);
                    def.bodyLines.push_back(line);
                }
            }
            else if (pos < json.size() && json[pos] == '"')
            {
                std::string body = readString(json, pos);
                def.bodyLines.push_back(body);
            }
            else
            {
                skipValue(json, pos);
            }
        }
        else
        {
            skipValue(json, pos);
        }
    }
    return false;
}

int main()
{
    std::ifstream file("assets/xell_snippets.json");
    if (!file.is_open())
    {
        std::cerr << "Cannot open file!\n";
        return 1;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    std::cerr << "File size: " << content.size() << " bytes\n";

    size_t pos = 0;
    skipWhitespace(content, pos);
    if (pos >= content.size() || content[pos] != '{')
    {
        std::cerr << "Not starting with {, char at pos " << pos << " is '" << content[pos] << "'\n";
        return 1;
    }
    pos++;

    int count = 0;
    while (pos < content.size())
    {
        skipWhitespace(content, pos);
        if (pos >= content.size() || content[pos] == '}')
            break;
        if (content[pos] == ',')
        {
            pos++;
            continue;
        }

        std::string snippetName = readString(content, pos);
        if (snippetName.empty())
        {
            std::cerr << "Empty snippet name at pos " << pos << ", char='" << content[pos] << "'\n";
            break;
        }

        skipWhitespace(content, pos);
        if (pos >= content.size() || content[pos] != ':')
        {
            std::cerr << "Expected : after name '" << snippetName << "' at pos " << pos << "\n";
            break;
        }
        pos++;

        SnippetDef def;
        if (!parseSnippetObject(content, pos, def))
        {
            std::cerr << "Failed to parse snippet object for '" << snippetName << "' at pos " << pos << "\n";
            // Show context
            size_t start = pos > 40 ? pos - 40 : 0;
            size_t end = std::min(pos + 40, content.size());
            std::cerr << "Context: ..." << content.substr(start, end - start) << "...\n";
            break;
        }

        if (!def.prefix.empty() && !def.bodyLines.empty())
        {
            count++;
            std::cout << count << ". " << def.prefix << " (" << def.bodyLines.size() << " lines)\n";
        }
    }

    std::cerr << "Total loaded: " << count << " snippets\n";
    return 0;
}
