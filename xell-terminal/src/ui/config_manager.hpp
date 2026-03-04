#pragma once

// =============================================================================
// config_manager.hpp — Configuration & Extensibility for the Xell Terminal IDE
// =============================================================================
// Phase 7: xell_ide.json config file, hot-reload, plugin system stub.
//
// Config location: ~/.config/xell/xell_ide.json
// All settings have sensible defaults — works without a config file.
// =============================================================================

#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <functional>
#include <cstdlib>
#include <chrono>
#include "visual_effects.hpp"
#include "../theme/theme_loader.hpp"

namespace xterm
{

    // =====================================================================
    // IDEConfig — All configurable settings
    // =====================================================================

    struct EditorConfig
    {
        int fontSize = 14;
        int tabSize = 4;
        bool wordWrap = false;
        bool lineNumbers = true;
        bool minimap = true;
        bool bracketMatching = true;
        bool indentGuides = true;
        std::string cursorStyle = "block"; // "block", "bar", "underline"
        bool cursorBlink = true;
        int cursorBlinkRate = 530;
        bool autoIndent = true;
        bool highlightCurrentLine = true;
        bool codeFolding = true;
        bool smoothScrolling = true;
        float smoothScrollSpeed = 0.3f;
    };

    struct PanelConfig
    {
        bool fileExplorerVisible = true;
        int fileExplorerWidth = 30;
        bool bottomPanelVisible = false;
        int bottomPanelHeight = 12;
        bool minimapVisible = true;
        int minimapWidth = 10;
    };

    struct REPLConfig
    {
        int historySize = 1000;
        bool autoExecuteOnEnter = false;
        bool shellPassthrough = true;
    };

    struct KeyBinding
    {
        std::string action;
        std::string keys; // e.g., "Ctrl+S", "Ctrl+Shift+B"
    };

    struct IDEConfig
    {
        EditorConfig editor;
        PanelConfig panels;
        REPLConfig repl;
        std::string themePath = "terminal_colors.json";
        std::vector<KeyBinding> keybindings;

        // Git integration
        bool gitEnabled = true;
        bool gitGutter = true;
        bool gitAutoRefresh = true;
        int gitRefreshIntervalMs = 5000;

        // Session
        bool restoreSession = true;
        int maxRecentFiles = 20;
        std::vector<std::string> recentFiles;
    };

    // =====================================================================
    // ConfigManager — Load, save, and hot-reload configuration
    // =====================================================================

    class ConfigManager
    {
    public:
        ConfigManager() { determineConfigPath(); }

        // ── Load configuration ──────────────────────────────────────

        bool load()
        {
            if (!std::filesystem::exists(configPath_))
            {
                // Use defaults, create config dir
                ensureConfigDir();
                save(); // Write default config
                return true;
            }

            std::ifstream file(configPath_);
            if (!file.is_open())
                return false;

            std::string content((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
            file.close();

            parseJson(content);
            lastModTime_ = getFileModTime(configPath_);
            return true;
        }

        // ── Save configuration ──────────────────────────────────────

        bool save()
        {
            ensureConfigDir();

            std::ofstream file(configPath_);
            if (!file.is_open())
                return false;

            file << generateJson();
            file.close();
            lastModTime_ = getFileModTime(configPath_);
            return true;
        }

        // ── Hot-reload check ────────────────────────────────────────

        bool checkForChanges()
        {
            if (!std::filesystem::exists(configPath_))
                return false;

            time_t modTime = getFileModTime(configPath_);
            if (modTime != lastModTime_)
            {
                lastModTime_ = modTime;
                load();
                if (onReload_)
                    onReload_(config_);
                return true;
            }
            return false;
        }

        // ── Callback for reload events ──────────────────────────────

        void setOnReload(std::function<void(const IDEConfig &)> callback)
        {
            onReload_ = callback;
        }

        // ── Access config ───────────────────────────────────────────

        IDEConfig &config() { return config_; }
        const IDEConfig &config() const { return config_; }

        // ── Apply config to visual effects ──────────────────────────

        void applyToEffects(VisualEffects &effects) const
        {
            // Cursor
            CursorConfig cursorCfg;
            if (config_.editor.cursorStyle == "bar")
                cursorCfg.shape = CursorShape::BAR;
            else if (config_.editor.cursorStyle == "underline")
                cursorCfg.shape = CursorShape::UNDERLINE;
            else
                cursorCfg.shape = CursorShape::BLOCK;
            cursorCfg.blink = config_.editor.cursorBlink;
            cursorCfg.blinkRateMs = config_.editor.cursorBlinkRate;
            effects.cursor.setConfig(cursorCfg);

            // Smooth scroll
            effects.smoothScroll.setEnabled(config_.editor.smoothScrolling);
            effects.smoothScroll.setSmoothSpeed(config_.editor.smoothScrollSpeed);

            // Minimap
            effects.minimap.setEnabled(config_.editor.minimap && config_.panels.minimapVisible);
            effects.minimap.setWidth(config_.panels.minimapWidth);

            // Bracket matching
            effects.bracketMatcher.setEnabled(config_.editor.bracketMatching);

            // Indent guides
            effects.indentGuides.setEnabled(config_.editor.indentGuides);
            effects.indentGuides.setTabSize(config_.editor.tabSize);

            // Code folding
            effects.codeFolding.setEnabled(config_.editor.codeFolding);
        }

        // ── Config file path ────────────────────────────────────────

        const std::string &configPath() const { return configPath_; }

        // ── Add recent file ─────────────────────────────────────────

        void addRecentFile(const std::string &path)
        {
            // Remove if already in list
            auto &recent = config_.recentFiles;
            recent.erase(std::remove(recent.begin(), recent.end(), path), recent.end());
            // Add to front
            recent.insert(recent.begin(), path);
            // Trim
            while ((int)recent.size() > config_.maxRecentFiles)
                recent.pop_back();
        }

    private:
        IDEConfig config_;
        std::string configPath_;
        time_t lastModTime_ = 0;
        std::function<void(const IDEConfig &)> onReload_;

        void determineConfigPath()
        {
#ifdef _WIN32
            const char *home = std::getenv("APPDATA");
            if (!home)
                home = std::getenv("USERPROFILE");
            if (home)
                configPath_ = std::string(home) + "\\xell\\xell_ide.json";
#else
            const char *home = std::getenv("HOME");
            if (home)
                configPath_ = std::string(home) + "/.config/xell/xell_ide.json";
#endif
            else
                configPath_ = "xell_ide.json"; // fallback
        }

        void ensureConfigDir()
        {
            std::filesystem::path dir = std::filesystem::path(configPath_).parent_path();
            if (!dir.empty())
                std::filesystem::create_directories(dir);
        }

        time_t getFileModTime(const std::string &path)
        {
            try
            {
                auto ftime = std::filesystem::last_write_time(path);
                auto sctp = std::chrono::time_point_cast<std::chrono::seconds>(
                    ftime - std::filesystem::file_time_type::clock::now() +
                    std::chrono::system_clock::now());
                return std::chrono::system_clock::to_time_t(sctp);
            }
            catch (...)
            {
                return 0;
            }
        }

        // ── Minimal JSON parser (reusing pattern from theme_loader) ──

        std::string trim(const std::string &s) const
        {
            size_t start = s.find_first_not_of(" \t\n\r");
            size_t end = s.find_last_not_of(" \t\n\r");
            if (start == std::string::npos)
                return "";
            return s.substr(start, end - start + 1);
        }

        std::string extractString(const std::string &json, const std::string &key) const
        {
            std::string search = "\"" + key + "\"";
            auto pos = json.find(search);
            if (pos == std::string::npos)
                return "";
            pos = json.find(':', pos + search.size());
            if (pos == std::string::npos)
                return "";
            pos = json.find('"', pos + 1);
            if (pos == std::string::npos)
                return "";
            auto end = json.find('"', pos + 1);
            if (end == std::string::npos)
                return "";
            return json.substr(pos + 1, end - pos - 1);
        }

        int extractInt(const std::string &json, const std::string &key, int def) const
        {
            std::string search = "\"" + key + "\"";
            auto pos = json.find(search);
            if (pos == std::string::npos)
                return def;
            pos = json.find(':', pos + search.size());
            if (pos == std::string::npos)
                return def;
            pos++;
            while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t'))
                pos++;
            try
            {
                return std::stoi(json.substr(pos));
            }
            catch (...)
            {
                return def;
            }
        }

        bool extractBool(const std::string &json, const std::string &key, bool def) const
        {
            std::string search = "\"" + key + "\"";
            auto pos = json.find(search);
            if (pos == std::string::npos)
                return def;
            pos = json.find(':', pos + search.size());
            if (pos == std::string::npos)
                return def;
            auto rest = trim(json.substr(pos + 1, 10));
            if (rest.substr(0, 4) == "true")
                return true;
            if (rest.substr(0, 5) == "false")
                return false;
            return def;
        }

        float extractFloat(const std::string &json, const std::string &key, float def) const
        {
            std::string search = "\"" + key + "\"";
            auto pos = json.find(search);
            if (pos == std::string::npos)
                return def;
            pos = json.find(':', pos + search.size());
            if (pos == std::string::npos)
                return def;
            pos++;
            while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t'))
                pos++;
            try
            {
                return std::stof(json.substr(pos));
            }
            catch (...)
            {
                return def;
            }
        }

        void parseJson(const std::string &content)
        {
            // Editor settings
            config_.editor.fontSize = extractInt(content, "font_size", 14);
            config_.editor.tabSize = extractInt(content, "tab_size", 4);
            config_.editor.wordWrap = extractBool(content, "word_wrap", false);
            config_.editor.lineNumbers = extractBool(content, "line_numbers", true);
            config_.editor.minimap = extractBool(content, "minimap", true);
            config_.editor.bracketMatching = extractBool(content, "bracket_matching", true);
            config_.editor.indentGuides = extractBool(content, "indent_guides", true);
            config_.editor.cursorStyle = extractString(content, "cursor_style");
            if (config_.editor.cursorStyle.empty())
                config_.editor.cursorStyle = "block";
            config_.editor.cursorBlink = extractBool(content, "cursor_blink", true);
            config_.editor.cursorBlinkRate = extractInt(content, "cursor_blink_rate", 530);
            config_.editor.autoIndent = extractBool(content, "auto_indent", true);
            config_.editor.highlightCurrentLine = extractBool(content, "highlight_current_line", true);
            config_.editor.codeFolding = extractBool(content, "code_folding", true);
            config_.editor.smoothScrolling = extractBool(content, "smooth_scrolling", true);
            config_.editor.smoothScrollSpeed = extractFloat(content, "smooth_scroll_speed", 0.3f);

            // Panel settings
            config_.panels.fileExplorerVisible = extractBool(content, "file_explorer_visible", true);
            config_.panels.fileExplorerWidth = extractInt(content, "file_explorer_width", 30);
            config_.panels.bottomPanelVisible = extractBool(content, "bottom_panel_visible", false);
            config_.panels.bottomPanelHeight = extractInt(content, "bottom_panel_height", 12);
            config_.panels.minimapVisible = extractBool(content, "minimap_visible", true);
            config_.panels.minimapWidth = extractInt(content, "minimap_width", 10);

            // REPL
            config_.repl.historySize = extractInt(content, "history_size", 1000);
            config_.repl.autoExecuteOnEnter = extractBool(content, "auto_execute_on_enter", false);
            config_.repl.shellPassthrough = extractBool(content, "shell_passthrough", true);

            // Theme
            config_.themePath = extractString(content, "theme");
            if (config_.themePath.empty())
                config_.themePath = "terminal_colors.json";

            // Git
            config_.gitEnabled = extractBool(content, "git_enabled", true);
            config_.gitGutter = extractBool(content, "git_gutter", true);
            config_.gitAutoRefresh = extractBool(content, "git_auto_refresh", true);
            config_.gitRefreshIntervalMs = extractInt(content, "git_refresh_interval", 5000);

            // Session
            config_.restoreSession = extractBool(content, "restore_session", true);
            config_.maxRecentFiles = extractInt(content, "max_recent_files", 20);
        }

        std::string generateJson() const
        {
            std::ostringstream ss;
            ss << "{\n";
            ss << "  \"editor\": {\n";
            ss << "    \"font_size\": " << config_.editor.fontSize << ",\n";
            ss << "    \"tab_size\": " << config_.editor.tabSize << ",\n";
            ss << "    \"word_wrap\": " << (config_.editor.wordWrap ? "true" : "false") << ",\n";
            ss << "    \"line_numbers\": " << (config_.editor.lineNumbers ? "true" : "false") << ",\n";
            ss << "    \"minimap\": " << (config_.editor.minimap ? "true" : "false") << ",\n";
            ss << "    \"bracket_matching\": " << (config_.editor.bracketMatching ? "true" : "false") << ",\n";
            ss << "    \"indent_guides\": " << (config_.editor.indentGuides ? "true" : "false") << ",\n";
            ss << "    \"cursor_style\": \"" << config_.editor.cursorStyle << "\",\n";
            ss << "    \"cursor_blink\": " << (config_.editor.cursorBlink ? "true" : "false") << ",\n";
            ss << "    \"cursor_blink_rate\": " << config_.editor.cursorBlinkRate << ",\n";
            ss << "    \"auto_indent\": " << (config_.editor.autoIndent ? "true" : "false") << ",\n";
            ss << "    \"highlight_current_line\": " << (config_.editor.highlightCurrentLine ? "true" : "false") << ",\n";
            ss << "    \"code_folding\": " << (config_.editor.codeFolding ? "true" : "false") << ",\n";
            ss << "    \"smooth_scrolling\": " << (config_.editor.smoothScrolling ? "true" : "false") << ",\n";
            ss << "    \"smooth_scroll_speed\": " << config_.editor.smoothScrollSpeed << "\n";
            ss << "  },\n";
            ss << "  \"theme\": \"" << config_.themePath << "\",\n";
            ss << "  \"panels\": {\n";
            ss << "    \"file_explorer_visible\": " << (config_.panels.fileExplorerVisible ? "true" : "false") << ",\n";
            ss << "    \"file_explorer_width\": " << config_.panels.fileExplorerWidth << ",\n";
            ss << "    \"bottom_panel_visible\": " << (config_.panels.bottomPanelVisible ? "true" : "false") << ",\n";
            ss << "    \"bottom_panel_height\": " << config_.panels.bottomPanelHeight << ",\n";
            ss << "    \"minimap_visible\": " << (config_.panels.minimapVisible ? "true" : "false") << ",\n";
            ss << "    \"minimap_width\": " << config_.panels.minimapWidth << "\n";
            ss << "  },\n";
            ss << "  \"repl\": {\n";
            ss << "    \"history_size\": " << config_.repl.historySize << ",\n";
            ss << "    \"auto_execute_on_enter\": " << (config_.repl.autoExecuteOnEnter ? "true" : "false") << ",\n";
            ss << "    \"shell_passthrough\": " << (config_.repl.shellPassthrough ? "true" : "false") << "\n";
            ss << "  },\n";
            ss << "  \"git\": {\n";
            ss << "    \"git_enabled\": " << (config_.gitEnabled ? "true" : "false") << ",\n";
            ss << "    \"git_gutter\": " << (config_.gitGutter ? "true" : "false") << ",\n";
            ss << "    \"git_auto_refresh\": " << (config_.gitAutoRefresh ? "true" : "false") << ",\n";
            ss << "    \"git_refresh_interval\": " << config_.gitRefreshIntervalMs << "\n";
            ss << "  },\n";
            ss << "  \"session\": {\n";
            ss << "    \"restore_session\": " << (config_.restoreSession ? "true" : "false") << ",\n";
            ss << "    \"max_recent_files\": " << config_.maxRecentFiles << "\n";
            ss << "  }\n";
            ss << "}\n";
            return ss.str();
        }
    };

    // =====================================================================
    // PluginStub — Future plugin system placeholder
    // =====================================================================

    struct PluginInfo
    {
        std::string name;
        std::string path; // path to .xel plugin file
        std::string version;
        std::string author;
        bool enabled = true;
    };

    class PluginManager
    {
    public:
        PluginManager() = default;

        // Scan plugin directory
        void scanPlugins()
        {
#ifdef _WIN32
            const char *home = std::getenv("APPDATA");
#else
            const char *home = std::getenv("HOME");
#endif
            if (!home)
                return;

#ifdef _WIN32
            std::string pluginDir = std::string(home) + "\\xell\\plugins";
#else
            std::string pluginDir = std::string(home) + "/.config/xell/plugins";
#endif
            if (!std::filesystem::exists(pluginDir))
                return;

            plugins_.clear();
            for (auto &entry : std::filesystem::directory_iterator(pluginDir))
            {
                if (entry.path().extension() == ".xel")
                {
                    PluginInfo info;
                    info.name = entry.path().stem().string();
                    info.path = entry.path().string();
                    info.version = "0.1";
                    info.enabled = true;
                    plugins_.push_back(info);
                }
            }
        }

        const std::vector<PluginInfo> &plugins() const { return plugins_; }
        int pluginCount() const { return (int)plugins_.size(); }

        // Future: loadPlugin(), unloadPlugin(), executePlugin()

    private:
        std::vector<PluginInfo> plugins_;
    };

} // namespace xterm
