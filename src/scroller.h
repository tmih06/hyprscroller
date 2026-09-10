#pragma once

#include <hyprland/src/layout/algorithm/tiled/scrolling/ScrollingAlgorithm.hpp>
#include <hyprland/src/pointer/PointerController.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <vector>
#include <algorithm>

class ScrollerLayout : public Layout::Tiled::CScrollingAlgorithm {
  public:
    ScrollerLayout()          = default;
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

    std::vector<WP<Layout::ITarget>> m_targets;
};
