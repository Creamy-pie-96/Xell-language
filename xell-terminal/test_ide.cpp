// =============================================================================
// test_editor.cpp — Compile-and-smoke test for the Xell IDE components
// =============================================================================
// Tests: TextBuffer, EditorView, EditorInput, EditorWidget, FileTree, Layout
// Run: cd xell-terminal && g++ -std=c++17 -Isrc -I../src test_ide.cpp ../src/lexer/lexer.cpp -o /tmp/test_ide && /tmp/test_ide
// =============================================================================

#include <cassert>
#include <iostream>
#include <fstream>

// Pull in all components
#include "src/editor/text_buffer.hpp"
#include "src/editor/editor_view.hpp"
#include "src/editor/editor_input.hpp"
#include "src/editor/editor_widget.hpp"
#include "src/ui/panel.hpp"
#include "src/ui/file_tree.hpp"
#include "src/ui/layout_manager.hpp"
#include "src/ui/autocomplete.hpp"
#include "src/ui/diagnostics.hpp"
#include "src/ui/find_replace.hpp"

using namespace xterm;

void testTextBuffer()
{
    std::cout << "  TextBuffer..." << std::flush;

    TextBuffer buf;

    // Initial state: one empty line
    assert(buf.lineCount() == 1);
    assert(buf.getLine(0) == "");

    // Insert characters
    BufferPos pos = {0, 0};
    buf.insertChar(pos, 'h');
    buf.insertChar({0, 1}, 'i');
    assert(buf.getLine(0) == "hi");

    // Insert text
    buf.insertText({0, 2}, " world");
    assert(buf.getLine(0) == "hi world");

    // Insert newline
    buf.insertNewline({0, 2});
    assert(buf.lineCount() == 2);
    assert(buf.getLine(0) == "hi");
    assert(buf.getLine(1) == " world");

    // Delete char
    buf.deleteCharBefore({1, 0}); // merge lines
    assert(buf.lineCount() == 1);
    assert(buf.getLine(0) == "hi world");

    // Undo
    assert(buf.canUndo());
    buf.undo();
    assert(buf.lineCount() == 2);

    // Redo
    assert(buf.canRedo());
    buf.redo();
    assert(buf.lineCount() == 1);

    // File save/load
    buf.insertText({0, 8}, "\nsecond line\nthird");
    std::string tmpFile = "/tmp/test_xell_editor.txt";
    assert(buf.saveToFile(tmpFile));

    TextBuffer buf2;
    assert(buf2.loadFromFile(tmpFile));
    assert(buf2.lineCount() == 3);
    assert(buf2.getLine(2) == "third");

    std::remove(tmpFile.c_str());

    std::cout << " OK" << std::endl;
}

void testEditorView()
{
    std::cout << "  EditorView..." << std::flush;

    TextBuffer buf;
    buf.insertText({0, 0}, "let x = 42\nlet y = 'hello'\nfn add(a, b):\n    ret a + b");

    ThemeData theme = loadDefaultTheme();
    EditorView view(buf, theme);
    view.setRect({0, 0, 80, 24});

    // Cursor starts at 0,0
    assert(view.cursor().row == 0);
    assert(view.cursor().col == 0);

    // Move cursor
    view.moveCursorDown();
    assert(view.cursor().row == 1);

    view.moveCursorEnd();
    assert(view.cursor().col == (int)buf.getLine(1).size());

    view.moveCursorHome();
    // Smart home goes to first non-whitespace (which is col 0 for this line)
    assert(view.cursor().col == 0);

    // Render produces output
    auto out = view.render();
    assert(!out.cells.empty());
    assert((int)out.cells.size() == 24);
    assert((int)out.cells[0].size() == 80);

    // Selection
    view.setCursor({0, 0});
    view.moveCursorRight(true); // select
    view.moveCursorRight(true);
    view.moveCursorRight(true);
    assert(view.selection().active);
    assert(view.getSelectedText() == "let");

    std::cout << " OK" << std::endl;
}

void testEditorViewEditing()
{
    std::cout << "  EditorView editing..." << std::flush;

    TextBuffer buf;
    ThemeData theme = loadDefaultTheme();
    EditorView view(buf, theme);
    view.setRect({0, 0, 80, 24});

    // Type some text
    view.insertChar('a');
    view.insertChar('b');
    view.insertChar('c');
    assert(buf.getLine(0) == "abc");
    assert(view.cursor().col == 3);

    // Backspace
    view.backspace();
    assert(buf.getLine(0) == "ab");

    // Newline with auto-indent
    buf.insertText({0, 0}, "");
    view.setCursor({0, 2});
    view.insertNewline();
    assert(buf.lineCount() == 2);

    // Undo
    view.undo();
    assert(buf.lineCount() == 1);

    std::cout << " OK" << std::endl;
}

void testEditorWidget()
{
    std::cout << "  EditorWidget..." << std::flush;

    ThemeData theme = loadDefaultTheme();
    EditorWidget widget(theme);

    // Starts with one untitled tab
    assert(widget.tabCount() == 1);

    // Resize
    widget.resize(120, 40);

    // Render
    auto out = widget.render();
    assert((int)out.cells.size() == 40);
    assert((int)out.cells[0].size() == 120);

    // Status info
    auto info = widget.getStatusInfo();
    assert(info.filename == "Untitled");
    assert(info.cursorRow == 1);
    assert(info.cursorCol == 1);
    assert(info.language == "Xell");

    // Create a temp file, open it
    std::string tmpFile = "/tmp/test_xell_widget.xel";
    {
        std::ofstream f(tmpFile);
        f << "let greeting = 'Hello Xell!'\nprint(greeting)\n";
    }

    widget.openFile(tmpFile);
    assert(widget.tabCount() == 2);

    info = widget.getStatusInfo();
    assert(info.filename == "test_xell_widget.xel");
    assert(info.totalLines >= 2); // 2 lines of content (+ possible trailing empty)

    // Close tab
    widget.closeTab();
    assert(widget.tabCount() == 1);

    std::remove(tmpFile.c_str());

    std::cout << " OK" << std::endl;
}

void testFileTree()
{
    std::cout << "  FileTree..." << std::flush;

    ThemeData theme = loadDefaultTheme();
    FileTreePanel tree(theme);

    tree.setRoot("/home/DATA/CODE/code/Xell");
    tree.setRect({0, 0, 30, 20});

    auto grid = tree.render();
    assert((int)grid.size() == 20);
    assert((int)grid[0].size() == 30);

    std::cout << " OK" << std::endl;
}

void testLayoutManager()
{
    std::cout << "  LayoutManager..." << std::flush;

    ThemeData theme = loadDefaultTheme();
    LayoutManager layout(theme);

    layout.resize(120, 40);
    layout.setProjectRoot("/home/DATA/CODE/code/Xell");

    auto out = layout.render();
    assert((int)out.cells.size() == 40);
    assert((int)out.cells[0].size() == 120);

    // Toggle sidebar
    assert(layout.sidebarVisible());
    layout.toggleSidebar();
    assert(!layout.sidebarVisible());
    layout.toggleSidebar();
    assert(layout.sidebarVisible());

    // Toggle bottom panel & focus cycling
    layout.toggleBottomPanel();
    assert(layout.bottomPanelVisible());

    assert(layout.focus() == FocusRegion::Editor);
    layout.cycleFocus();
    assert(layout.focus() == FocusRegion::BottomPanel);
    layout.cycleFocus();
    assert(layout.focus() == FocusRegion::Sidebar);

    std::cout << " OK" << std::endl;
}

void testAutocomplete()
{
    std::cout << "  Autocomplete..." << std::flush;

    CompletionDB db;
    db.loadDefaults();

    // Test keyword matching
    auto results = db.match("fn");
    assert(!results.empty());
    assert(results[0].label == "fn");

    // Test builtin matching
    results = db.match("len");
    assert(!results.empty());
    bool foundLen = false;
    for (auto &r : results)
        if (r.label == "len")
            foundLen = true;
    assert(foundLen);

    // Test fuzzy matching
    results = db.match("pr");
    assert(!results.empty()); // should match "print", "property"

    // Test user symbols
    std::vector<std::string> code = {"fn greet(name):", "    print(name)", ";",
                                      "let x = 42", "class Animal:"};
    db.scanBuffer(code);
    results = db.match("gre");
    bool foundGreet = false;
    for (auto &r : results)
        if (r.label == "greet")
            foundGreet = true;
    assert(foundGreet);

    // Test popup rendering
    ThemeData theme = loadDefaultTheme();
    AutocompletePopup popup(theme);
    popup.show(5, 10, "fn", db);
    assert(popup.isVisible());

    auto render = popup.render();
    assert(render.h > 0);
    assert(render.w > 0);

    popup.hide();
    assert(!popup.isVisible());

    std::cout << " OK" << std::endl;
}

void testDiagnostics()
{
    std::cout << "  Diagnostics..." << std::flush;

    ThemeData theme = loadDefaultTheme();
    DiagnosticEngine engine(theme);

    // Manually add diagnostics
    engine.addDiagnostic({3, 0, -1, "undefined variable 'x'", "analyzer", DiagnosticSeverity::Error});
    engine.addDiagnostic({7, 0, -1, "unused variable 'y'", "analyzer", DiagnosticSeverity::Warning});

    assert(engine.errorCount() == 1);
    assert(engine.warningCount() == 1);
    assert(engine.diagnostics().size() == 2);

    assert(engine.gutterMarker(3) == 'E');
    assert(engine.gutterMarker(7) == 'W');
    assert(engine.gutterMarker(0) == ' ');

    auto forLine3 = engine.diagnosticsForLine(3);
    assert(forLine3.size() == 1);
    assert(forLine3[0]->message == "undefined variable 'x'");

    // Render diagnostic list
    auto list = engine.renderList(80, 10);
    assert((int)list.cells.size() == 10);
    assert((int)list.cells[0].size() == 80);

    // Clear
    engine.clear();
    assert(engine.diagnostics().empty());

    std::cout << " OK" << std::endl;
}

void testFindReplace()
{
    std::cout << "  FindReplace..." << std::flush;

    ThemeData theme = loadDefaultTheme();
    FindReplace finder(theme);

    // Create a buffer with some content
    TextBuffer buf;
    buf.insertText({0, 0}, "let x = 42\nlet y = x + 1\nlet z = x * y\nprint(x)");

    // Find all occurrences of "x"
    finder.setFindText("x");
    finder.findAll(buf);
    assert(finder.matchCount() == 4); // x appears 4 times

    // Navigate
    auto *match = finder.currentMatchResult();
    assert(match != nullptr);
    assert(match->row == 0);
    assert(match->colStart == 4); // "let x = 42" -> x at col 4

    finder.nextMatch();
    match = finder.currentMatchResult();
    assert(match->row == 1);

    // Case sensitivity
    buf.insertText({0, 0}, ""); // no change
    finder.setFindText("LET");
    finder.findAll(buf);
    assert(finder.matchCount() == 3); // case-insensitive by default
    finder.toggleCaseSensitive();
    finder.findAll(buf);
    assert(finder.matchCount() == 0); // "LET" not found case-sensitive
    finder.toggleCaseSensitive(); // back to insensitive

    // Replace
    finder.setFindText("x");
    finder.setReplaceText("val");
    finder.findAll(buf);
    int replaced = finder.replaceAll(buf);
    assert(replaced == 4);
    // Verify replacements
    assert(buf.getLine(0).find("val") != std::string::npos);

    // Render
    finder.show(true);
    auto render = finder.render(80);
    assert(render.height == 2); // find + replace rows
    assert((int)render.cells[0].size() == 80);

    finder.hide();
    assert(!finder.isVisible());

    std::cout << " OK" << std::endl;
}

int main()
{
    std::cout << "=== Xell IDE Tests ===" << std::endl;

    testTextBuffer();
    testEditorView();
    testEditorViewEditing();
    testEditorWidget();
    testFileTree();
    testLayoutManager();
    testAutocomplete();
    testDiagnostics();
    testFindReplace();

    std::cout << "\nAll IDE tests passed! ✓" << std::endl;
    return 0;
}
