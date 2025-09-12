#pragma once
#ifndef __WINDOWUI_H__
#define __WINDOWUI_H__

namespace DSM {
    struct WindowUI
    {
        virtual void Render() = 0;
    };
} // namespace DSM

#endif