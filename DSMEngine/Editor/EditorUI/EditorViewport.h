#pragma once
#ifndef __EDITORVIEWPORT_H__
#define __EDITORVIEWPORT_H__


#include "Editor/EditorUI/Widget.h"

namespace DSM {
    class EditorViewport : public Widget
    {
    public:
        EditorViewport(EditorUI* editorUI);

        void OnGUIEnabled() override;
    
    private:
        int m_GizmoType = -1;
    };
}

#endif // __EDITORVIEWPORT_H__