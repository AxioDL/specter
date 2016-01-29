#include "Specter/RootView.hpp"
#include "Specter/ViewResources.hpp"
#include "Specter/Space.hpp"
#include "Specter/Menu.hpp"

namespace Specter
{
static LogVisor::LogModule Log("Specter::RootView");

RootView::RootView(IViewManager& viewMan, ViewResources& res, boo::IWindow* window)
: View(res), m_window(window), m_viewMan(viewMan), m_viewRes(&res), m_events(*this),
  m_splitMenuNode(*this)
{
    window->setCallback(&m_events);
    boo::SWindowRect rect = window->getWindowFrame();
    m_renderTex = res.m_factory->newRenderTexture(rect.size[0], rect.size[1], 1);
    commitResources(res);
    resized(rect, rect);
}

void RootView::destroyed()
{
    m_destroyed = true;
}

void RootView::resized(const boo::SWindowRect& root, const boo::SWindowRect&)
{
    m_rootRect = root;
    m_rootRect.location[0] = 0;
    m_rootRect.location[1] = 0;
    View::resized(m_rootRect, m_rootRect);
    for (View* v : m_views)
        v->resized(m_rootRect, m_rootRect);
    if (m_tooltip)
        m_tooltip->resized(m_rootRect, m_rootRect);
    if (m_rightClickMenu.m_view)
    {
         float wr = root.size[0] / float(m_rightClickMenuRootAndLoc.size[0]);
         float hr = root.size[1] / float(m_rightClickMenuRootAndLoc.size[1]);
         m_rightClickMenuRootAndLoc.size[0] = root.size[0];
         m_rightClickMenuRootAndLoc.size[1] = root.size[1];
         m_rightClickMenuRootAndLoc.location[0] *= wr;
         m_rightClickMenuRootAndLoc.location[1] *= hr;
         m_rightClickMenu.m_view->resized(root, m_rightClickMenuRootAndLoc);
    }
    m_resizeRTDirty = true;
}

void RootView::mouseDown(const boo::SWindowCoord& coord, boo::EMouseButton button, boo::EModifierKey mods)
{
    if (m_rightClickMenu.m_view)
    {
        if (!m_rightClickMenu.mouseDown(coord, button, mods))
            m_rightClickMenu.m_view.reset();
        return;
    }

    if (m_activeMenuButton)
    {
        ViewChild<std::unique_ptr<View>>& mv = m_activeMenuButton->getMenu();
        if (!mv.mouseDown(coord, button, mods))
            m_activeMenuButton->closeMenu(coord);
        return;
    }

    if (m_hoverSplitDragView)
    {
        if (button == boo::EMouseButton::Primary)
        {
            m_activeSplitDragView = true;
            m_hoverSplitDragView->startDragSplit(coord);
        }
        else if (button == boo::EMouseButton::Secondary)
        {
            m_splitMenuNode.m_splitView = m_hoverSplitDragView;
            adoptRightClickMenu(std::make_unique<Specter::Menu>(*m_viewRes, *this, &m_splitMenuNode), coord);
        }
        return;
    }

    if (m_activeTextView && !m_activeTextView->subRect().coordInRect(coord))
        setActiveTextView(nullptr);
    for (View* v : m_views)
        v->mouseDown(coord, button, mods);
}

void RootView::mouseUp(const boo::SWindowCoord& coord, boo::EMouseButton button, boo::EModifierKey mods)
{
    if (m_rightClickMenu.m_view)
    {
        m_rightClickMenu.mouseUp(coord, button, mods);
        return;
    }

    if (m_activeMenuButton)
    {
        ViewChild<std::unique_ptr<View>>& mv = m_activeMenuButton->getMenu();
        mv.mouseUp(coord, button, mods);
        return;
    }

    if (m_activeSplitDragView && button == boo::EMouseButton::Primary)
    {
        m_activeSplitDragView = false;
        m_hoverSplitDragView->endDragSplit();
        m_spaceCornerHover = false;
        m_hSplitHover = false;
        m_vSplitHover = false;
        _updateCursor();
    }

    for (View* v : m_views)
        v->mouseUp(coord, button, mods);
}

SplitView* RootView::recursiveTestSplitHover(SplitView* sv, const boo::SWindowCoord& coord) const
{
    if (sv->testSplitHover(coord))
        return sv;
    for (int i=0 ; i<2 ; ++i)
    {
        SplitView* child = dynamic_cast<SplitView*>(sv->m_views[i].m_view);
        if (child)
        {
            SplitView* res = recursiveTestSplitHover(child, coord);
            if (res)
                return res;
        }
    }
    return nullptr;
}

void RootView::mouseMove(const boo::SWindowCoord& coord)
{
    if (m_rightClickMenu.m_view)
    {
        m_hSplitHover = false;
        m_vSplitHover = false;
        _updateCursor();
        m_rightClickMenu.mouseMove(coord);
        return;
    }

    if (m_activeMenuButton)
    {
        m_hSplitHover = false;
        m_vSplitHover = false;
        _updateCursor();
        ViewChild<std::unique_ptr<View>>& mv = m_activeMenuButton->getMenu();
        mv.mouseMove(coord);
        return;
    }

    if (m_activeSplitDragView)
    {
        m_hoverSplitDragView->moveDragSplit(coord);
        m_spaceCornerHover = false;
        if (m_hoverSplitDragView->axis() == SplitView::Axis::Horizontal)
            setHorizontalSplitHover(true);
        else
            setVerticalSplitHover(true);
        return;
    }

    m_hoverSplitDragView = nullptr;
    if (!m_spaceCornerHover)
    {
        for (View* v : m_views)
        {
            SplitView* sv = dynamic_cast<SplitView*>(v);
            if (sv)
                sv = recursiveTestSplitHover(sv, coord);
            if (sv)
            {
                if (sv->axis() == SplitView::Axis::Horizontal)
                    setHorizontalSplitHover(true);
                else
                    setVerticalSplitHover(true);
                m_hoverSplitDragView = sv;
                break;
            }
            else
            {
                m_hSplitHover = false;
                m_vSplitHover = false;
                _updateCursor();
            }
        }
    }

    if (m_activeDragView)
        m_activeDragView->mouseMove(coord);
    else
    {
        for (View* v : m_views)
            v->mouseMove(coord);
    }

    boo::SWindowRect ttrect = m_rootRect;
    ttrect.location[0] = coord.pixel[0];
    ttrect.location[1] = coord.pixel[1];
    if (m_tooltip)
    {
        if (coord.pixel[0] + m_tooltip->nominalWidth() > m_rootRect.size[0])
            ttrect.location[0] -= m_tooltip->nominalWidth();
        if (coord.pixel[1] + m_tooltip->nominalHeight() > m_rootRect.size[1])
            ttrect.location[1] -= m_tooltip->nominalHeight();
        m_tooltip->resized(m_rootRect, ttrect);
    }
}

void RootView::mouseEnter(const boo::SWindowCoord& coord)
{
    for (View* v : m_views)
        v->mouseEnter(coord);
}

void RootView::mouseLeave(const boo::SWindowCoord& coord)
{
    if (m_rightClickMenu.m_view)
    {
        m_rightClickMenu.mouseLeave(coord);
        return;
    }

    if (m_activeMenuButton)
    {
        ViewChild<std::unique_ptr<View>>& mv = m_activeMenuButton->getMenu();
        mv.mouseLeave(coord);
        return;
    }

    for (View* v : m_views)
        v->mouseLeave(coord);
}

void RootView::scroll(const boo::SWindowCoord& coord, const boo::SScrollDelta& scroll)
{
    if (m_activeMenuButton)
    {
        ViewChild<std::unique_ptr<View>>& mv = m_activeMenuButton->getMenu();
        mv.scroll(coord, scroll);
        return;
    }

    for (View* v : m_views)
        v->scroll(coord, scroll);
}

void RootView::touchDown(const boo::STouchCoord& coord, uintptr_t tid)
{
    for (View* v : m_views)
        v->touchDown(coord, tid);
}

void RootView::touchUp(const boo::STouchCoord& coord, uintptr_t tid)
{
    for (View* v : m_views)
        v->touchUp(coord, tid);
}

void RootView::touchMove(const boo::STouchCoord& coord, uintptr_t tid)
{
    for (View* v : m_views)
        v->touchMove(coord, tid);
}

void RootView::charKeyDown(unsigned long charCode, boo::EModifierKey mods, bool isRepeat)
{
    for (View* v : m_views)
        v->charKeyDown(charCode, mods, isRepeat);
    if (m_activeTextView &&
        (mods & (boo::EModifierKey::Ctrl|boo::EModifierKey::Command)) != boo::EModifierKey::None)
    {
        if (charCode == 'c' || charCode == 'C')
            m_activeTextView->clipboardCopy();
        else if (charCode == 'x' || charCode == 'X')
            m_activeTextView->clipboardCut();
        else if (charCode == 'v' || charCode == 'V')
            m_activeTextView->clipboardPaste();
    }
}

void RootView::charKeyUp(unsigned long charCode, boo::EModifierKey mods)
{
    for (View* v : m_views)
        v->charKeyUp(charCode, mods);
}

void RootView::specialKeyDown(boo::ESpecialKey key, boo::EModifierKey mods, bool isRepeat)
{
    if (key == boo::ESpecialKey::Enter && (mods & boo::EModifierKey::Alt) != boo::EModifierKey::None)
    {
        m_window->setFullscreen(!m_window->isFullscreen());
        return;
    }
    for (View* v : m_views)
        v->specialKeyDown(key, mods, isRepeat);
    if (m_activeTextView)
        m_activeTextView->specialKeyDown(key, mods, isRepeat);
}

void RootView::specialKeyUp(boo::ESpecialKey key, boo::EModifierKey mods)
{
    for (View* v : m_views)
        v->specialKeyUp(key, mods);
    if (m_activeTextView)
        m_activeTextView->specialKeyUp(key, mods);
}

void RootView::modKeyDown(boo::EModifierKey mod, bool isRepeat)
{
    for (View* v : m_views)
        v->modKeyDown(mod, isRepeat);
    if (m_activeTextView)
        m_activeTextView->modKeyDown(mod, isRepeat);
}

void RootView::modKeyUp(boo::EModifierKey mod)
{
    for (View* v : m_views)
        v->modKeyUp(mod);
    if (m_activeTextView)
        m_activeTextView->modKeyUp(mod);
}

void RootView::resetTooltip(ViewResources& res)
{
    m_tooltip.reset(new Tooltip(res, *this, "Test", "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Morbi hendrerit nisl quis lobortis mattis. Mauris efficitur, est a vestibulum iaculis, leo orci pellentesque nunc, non rutrum ipsum lectus eget nisl. Aliquam accumsan vestibulum turpis. Duis id lacus ac lectus sollicitudin posuere vel sit amet metus. Aenean nec tortor id enim efficitur accumsan vitae eu ante. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Fusce magna eros, lacinia a leo eget, volutpat rhoncus urna."));
}

void RootView::displayTooltip(const std::string& name, const std::string& help)
{
}

void RootView::draw(boo::IGraphicsCommandQueue* gfxQ)
{
    if (m_resizeRTDirty)
    {
        gfxQ->resizeRenderTexture(m_renderTex, m_rootRect.size[0], m_rootRect.size[1]);
        m_resizeRTDirty = false;
        gfxQ->schedulePostFrameHandler([&](){m_events.m_resizeCv.notify_one();});
    }
    gfxQ->setRenderTarget(m_renderTex);
    gfxQ->setViewport(m_rootRect);
    gfxQ->setScissor(m_rootRect);
    View::draw(gfxQ);
    for (View* v : m_views)
        v->draw(gfxQ);
    if (m_tooltip)
        m_tooltip->draw(gfxQ);
    if (m_rightClickMenu.m_view)
        m_rightClickMenu.m_view->draw(gfxQ);
    gfxQ->resolveDisplay(m_renderTex);
}

}
