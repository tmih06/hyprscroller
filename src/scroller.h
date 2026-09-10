#pragma once

#include <hyprland/src/layout/algorithm/tiled/scrolling/ScrollingAlgorithm.hpp>
#include <hyprland/src/layout/algorithm/Algorithm.hpp>
#include <hyprland/src/layout/space/Space.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/pointer/PointerController.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/output/Monitor.hpp>
#include <vector>
#include <algorithm>
#include <chrono>

class ScrollerLayout : public Layout::Tiled::CScrollingAlgorithm {
  public:
    ScrollerLayout() {
        m_mouseMoveListener = Event::bus()->m_events.input.mouse.move.listen([this](const Vector2D& pos, Event::SCallbackInfo&) {
            handleMouseMove(pos);
        });
    }

    virtual ~ScrollerLayout() = default;

    virtual std::optional<std::string> layoutName() const override {
        return "scroller";
    }

    virtual void newTarget(SP<Layout::ITarget> target) override {
        if (!target)
            return;

        m_targets.push_back(target);
        cleanupTargets();

        Layout::Tiled::CScrollingAlgorithm::newTarget(target);

        applyDynamicResizing();

        if (target->window()) {
            Pointer::pointerController()->warpTo(target->window()->middle());
        }
    }

    virtual void removeTarget(SP<Layout::ITarget> target) override {
        std::erase_if(m_targets, [target](const auto& wp) {
            auto sp = wp.lock();
            return !sp || sp == target;
        });
        cleanupTargets();

        Layout::Tiled::CScrollingAlgorithm::removeTarget(target);

        if (!Desktop::focusState()->window() && !m_targets.empty()) {
            for (auto it = m_targets.rbegin(); it != m_targets.rend(); ++it) {
                auto sp = it->lock();
                if (sp && sp->window()) {
                    Desktop::focusState()->fullWindowFocus(sp->window(), Desktop::FOCUS_REASON_UNMAP_WINDOW_TILING);
                    break;
                }
            }
        }

        applyDynamicResizing();
    }

    void applyDynamicResizing() {
        cleanupTargets();
        const size_t count = m_targets.size();

        if (count == 1) {
            (void)layoutMsg("colresize all 1.0");
            (void)layoutMsg("fit all");
        } else if (count == 2) {
            (void)layoutMsg("colresize all 0.5");
            (void)layoutMsg("fit all");
        } else if (count >= 3) {
            (void)layoutMsg("colresize all 0.333333");
            (void)layoutMsg("fit_into_view");
        }
    }

  private:
    void cleanupTargets() {
        std::erase_if(m_targets, [](const auto& wp) { return wp.expired(); });
    }

    void handleMouseMove(const Vector2D& pos) {
        cleanupTargets();
        if (m_targets.size() < 3)
            return;

        auto parent = m_parent.lock();
        if (!parent)
            return;

        auto sp = parent->space();
        if (!sp || !sp->workspace())
            return;

        auto mon = sp->workspace()->m_monitor.lock();
        if (!mon)
            return;

        const double leftEdge  = mon->m_position.x;
        const double rightEdge = mon->m_position.x + mon->m_size.x - 1.0;

        const auto now     = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastEdgeScrollTime).count();
        if (elapsed < 350)
            return;

        if (pos.x <= leftEdge + 5.0) {
            m_lastEdgeScrollTime = now;
            (void)layoutMsg("focus l");
        } else if (pos.x >= rightEdge - 5.0) {
            m_lastEdgeScrollTime = now;
            (void)layoutMsg("focus r");
        }
    }

    std::vector<WP<Layout::ITarget>>      m_targets;
    CHyprSignalListener                   m_mouseMoveListener;
    std::chrono::steady_clock::time_point m_lastEdgeScrollTime = std::chrono::steady_clock::now();
};
