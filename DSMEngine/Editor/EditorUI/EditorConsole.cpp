#include "EditorConsole.h"
#include "EditorStyle.h"
#include "Runtime/DSMEngine.h"
#include "Runtime/Core/Macro.h"
#include "Runtime/Render/TextureManager.h"

#include <imgui.h>

namespace DSM {
    constexpr size_t g_MaxLogCount = 1000;
    std::mutex g_LogMutex;
    std::deque<std::pair<LogSystem::LogLevel, std::string>> g_Logs;
    std::array<size_t, LogSystem::Count> g_LogLevelCount;

    std::array<bool, LogSystem::Count> g_LogLevelEnabled;
    const std::array<const ImVec4* const, LogSystem::Count> g_LogLevelColors = {
        &EditorStyle::sm_ColorInfo, &EditorStyle::sm_ColorInfo,
        &EditorStyle::sm_ColorInfo, &EditorStyle::sm_ColorWarning,
        &EditorStyle::sm_ColorError, &EditorStyle::sm_ColorInfo
    };

    EditorConsole::EditorConsole()
    {
        m_Title = "Console";
        m_Icon = TextureManager::LoadTextureFromFile("Textures/Icons/console.png");
        
        g_LogLevelEnabled.fill(true);
        DSMEngine::sm_GlobalContext.loggerSystem->SetLogFunc(EditorConsole::Log);
    }

    EditorConsole::~EditorConsole()
    {
        DSMEngine::sm_GlobalContext.loggerSystem->SetLogFunc(nullptr);
    }

    void EditorConsole::OnGUIEnabled()
    {
        if(ImGui::Button("Clear")){
            std::lock_guard lock{g_LogMutex};
            g_Logs.clear();
            g_Logs.shrink_to_fit();
            g_LogLevelCount.fill(0);
        }

        ImGui::SameLine();

        auto logLevelEnabled = [this](LogSystem::LogLevel level){
            bool& enabled = g_LogLevelEnabled[level];
            const auto& colors = ImGui::GetStyle().Colors;

            ImGui::PushStyleColor(ImGuiCol_Button, enabled ? colors[ImGuiCol_ButtonActive] : colors[ImGuiCol_Button]);
            std::string levelID = typeid(level).name() + level;
            // TODO: 后续使用通用的描述符
            auto texHandle = m_Icon->GetNativeView(ObjectTypes::D3D12_ShaderResourceViewGpuDescriptor);
            if(ImGui::ImageButton(levelID.c_str(), ImTextureRef{texHandle}, ImVec2(15,15), 
                ImVec2(0,0), ImVec2(1,1), ImVec4(0,0,0,0), *g_LogLevelColors[level])){
                enabled = !enabled;
            }
            ImGui::PopStyleColor();

            ImGui::SameLine();
            ImGui::Text("%d", g_LogLevelCount[level]);
            ImGui::SameLine();
        };

        logLevelEnabled(LogSystem::LogLevel::Info);
        logLevelEnabled(LogSystem::LogLevel::Warn);
        logLevelEnabled(LogSystem::LogLevel::Error);

        ImGuiTextFilter logFilter{};
        const float labelWidth = 37.0f;
        logFilter.Draw("Filter", ImGui::GetContentRegionAvail().x - labelWidth);
        ImGui::Separator();

        static const ImGuiTableFlags table_flags =
            ImGuiTableFlags_RowBg        |
            ImGuiTableFlags_BordersOuter |
            ImGuiTableFlags_ScrollX      |
            ImGuiTableFlags_ScrollY;

        if(ImGui::BeginTable("#Console Content", 1, table_flags, ImVec2(-1, -1))){
            for(size_t i = 0; i < g_Logs.size(); ++i){
                const auto& [level, text] = g_Logs[i];
                // 过滤日志
                if(logFilter.PassFilter(text.c_str()) && g_LogLevelEnabled[level]){
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);

                    {
                        ImGui::PushID(i);
                        
                        // 显示日志内容
                        ImGui::PushStyleColor(ImGuiCol_Text, *g_LogLevelColors[level]);
                        ImGui::TextUnformatted(text.c_str());
                        ImGui::PopStyleColor();

                        // 右键菜单
                        if (ImGui::BeginPopupContextItem("##widget_console_contextMenu")) {
                            // 复制日志内容
                            if (ImGui::MenuItem("Copy")) {
                                ImGui::LogToClipboard();
                                ImGui::LogText("%s", text.c_str());
                                ImGui::LogFinish();
                            }
                            ImGui::Separator();
                            ImGui::EndPopup();
                        }

                        ImGui::PopID();
                    }
                }
            }
            ImGui::EndTable();
        }
    }
    
    void EditorConsole::Log(LogSystem::LogLevel level, const std::string &text)
    {
        std::lock_guard lock{g_LogMutex};

        g_Logs.emplace_back(level, text);
        if(g_Logs.size() > g_MaxLogCount){
            g_Logs.pop_front();
        }

        g_LogLevelCount[level]++;
    }
}