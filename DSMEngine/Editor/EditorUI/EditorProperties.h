#pragma once
#ifndef __EDITOR_PROPERTIES_H__
#define __EDITOR_PROPERTIES_H__

#include "Editor/EditorUI/Widget.h"
#include "Editor/EditorUI/ComponentDrawer.h"

namespace DSM {
    class EditorProperties : public Widget
    {
    public:
        EditorProperties(EditorUI* editorUI);

        void OnGUIEnabled() override;

    private:
        std::unique_ptr<ComponentDrawerManager> m_ComponentDrawerManager;
    };
}


#endif // __EDITOR_PROPERTIES_H__