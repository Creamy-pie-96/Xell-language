#include <cassert>
#include <iostream>
#include "editor/text_buffer.hpp"
#include "ui/visual_effects.hpp"
using namespace xterm;
int main()
{
    BracketMatcher matcher;
    TextBuffer buf;
    buf.insertText({0, 0}, "fn add(a, b):\n    give a + b\n;");

    std::cout << "Lines: " << buf.lineCount() << std::endl;
    for (int i = 0; i < buf.lineCount(); i++)
        std::cout << "  [" << i << "] '" << buf.getLine(i) << "'" << std::endl;

    std::string line0 = buf.getLine(0);
    std::cout << "line0.size()=" << line0.size() << " char[13]='" << (line0.size() > 13 ? line0[13] : '?') << "'" << std::endl;

    auto pair = matcher.findMatch(buf, 0, 13);
    std::cout << "openLine=" << pair.openLine << " openCol=" << pair.openCol << std::endl;
    std::cout << "closeLine=" << pair.closeLine << " closeCol=" << pair.closeCol << std::endl;
    return 0;
}
