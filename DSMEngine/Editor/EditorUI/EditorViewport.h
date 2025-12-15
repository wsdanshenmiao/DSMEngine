#pragma once
#ifndef __EDITORVIEWPORT_H__
#define __EDITORVIEWPORT_H__


#include "Editor/EditorUI/Widget.h"

namespace DSM {
    class EditorViewport : public Widget
    {
    public:
        EditorViewport();

        void OnGUIEnabled() override;
    };
}

#endif // __EDITORVIEWPORT_H__