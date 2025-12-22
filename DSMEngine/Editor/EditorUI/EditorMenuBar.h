#pragma once
#ifndef __EDITORMENUBAR_H__
#define __EDITORMENUBAR_H__


namespace DSM {
    class EditorUI;

    class EditorMenuBar
    {
    public:
        EditorMenuBar(EditorUI* editorUI);
        void OnGUI();

    private:
        void ProjectMenuGUI();
        void WorldMenuGUI();
        void ViewMenuGUI();

    private:
        EditorUI* m_EditorUI;
    };
}

#endif