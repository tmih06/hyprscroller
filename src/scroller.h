#pragma once

#include <hyprland/src/layout/algorithm/tiled/scrolling/ScrollingAlgorithm.hpp>
#include <hyprland/src/pointer/PointerController.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <vector>
#include <cmath>
#include <algorithm>

class ScrollerLayout : public Layout::Tiled::CScrollingAlgorithm {
  public:
    ScrollerLayout()          = default;
    virtual ~ScrollerLayout() = default;

    virtual std::optional<std::string> layoutName() const override {
        return "scroller";
    }

    // Intercept mouse drag resizing
    virtual void resizeTarget(const Vector2D& Δ, SP<Layout::ITarget> target, Layout::eRectCorner corner = Layout::CORNER_NONE) override {
        if (target && m_targets.size() > 1) {
            markExplicitlyResized(target);
        }
        Layout::Tiled::CScrollingAlgorithm::resizeTarget(Δ, target, corner);
    }

    // Intercept keyboard/command colresize
    virtual Config::ErrorResult layoutMsg(const std::string_view& sv) override {
        if (sv.starts_with("colresize +") || sv.starts_with("colresize -") || sv.starts_with("colresize 0.")) {
            auto focused = Desktop::focusState()->window();
            if (focused && focused->layoutTarget() && m_targets.size() > 1) {
                markExplicitlyResized(focused->layoutTarget());
            }
        }
        return Layout::Tiled::CScrollingAlgorithm::layoutMsg(sv);
    }

    virtual void newTarget(SP<Layout::ITarget> target) override {
        if (!target)
            return;

        m_targets.push_back(target);
        cleanupTargets();

        Layout::Tiled::CScrollingAlgorithm::newTarget(target);

        applyDynamicResizing(target);

        if (target->window()) {
            Pointer::pointerController()->warpTo(target->window()->middle());
        }
    }

    virtual void removeTarget(SP<Layout::ITarget> target) override {
        std::erase_if(m_targets, [target](const auto& wp) {
            auto sp = wp.lock();
            return !sp || sp == target;
        });
        std::erase_if(m_explicitlyResized, [target](const auto& wp) {
            auto sp = wp.lock();
            return !sp || sp == target;
        });
        cleanupTargets();

        Layout::Tiled::CScrollingAlgorithm::removeTarget(target);

        if (m_targets.size() <= 1) {
            m_explicitlyResized.clear();
        }

        if (!Desktop::focusState()->window() && !m_targets.empty()) {
            for (auto it = m_targets.rbegin(); it != m_targets.rend(); ++it) {
                auto sp = it->lock();
                if (sp && sp->window()) {
                    Desktop::focusState()->fullWindowFocus(sp->window(), Desktop::FOCUS_REASON_UNMAP_WINDOW_TILING);
                    break;
                }
            }
        }

        applyDynamicResizing(nullptr);
    }

    void applyDynamicResizing(SP<Layout::ITarget> newTargetPtr) {
        cleanupTargets();
        const size_t count = m_targets.size();

        if (count <= 1) {
            // Single window always resets any previous multi-column split state and fills the screen
            m_explicitlyResized.clear();
            (void)layoutMsg("fit all");
        } else if (count == 2) {
            // Check if any window was explicitly customized by user while in multi-window state
            bool anyCustom = false;
            for (const auto& wt : m_targets) {
                if (isExplicitlyResized(wt.lock())) {
                    anyCustom = true;
                    break;
                }
            }

            if (!anyCustom) {
                // Both are default: split 50/50 and reset camera offset so both fit on screen
                (void)layoutMsg("fit all");
            } else {
                (void)layoutMsg("fit_into_view");
            }
        } else if (count >= 3) {
            // New window gets default 1/3 width
            if (newTargetPtr && !isExplicitlyResized(newTargetPtr)) {
                setTargetColumnWidth(newTargetPtr, 0.333333f);
            }
            // Existing windows: if at 0.5 (from 2-window state) and not custom, adjust to 0.333333 so 3 fit
            for (const auto& wt : m_targets) {
                auto t = wt.lock();
                if (t && t != newTargetPtr && !isExplicitlyResized(t)) {
                    auto data = dataFor(t);
                    if (data) {
                        auto col = data->column.lock();
                        if (col && std::abs(col->getColumnWidth() - 0.5f) < 0.05f) {
                            col->setColumnWidth(0.333333f);
                        }
                    }
                }
            }
            recalculate();
            (void)layoutMsg("fit_into_view");
        }
    }

  private:
    void markExplicitlyResized(SP<Layout::ITarget> t) {
        if (m_targets.size() <= 1)
            return;
        if (!isExplicitlyResized(t)) {
            m_userModifiedPush(t);
        }
    }

    void m_userModifiedPush(SP<Layout::ITarget> t) {
        m_explicitlyResized.push_back(t);
    }

    bool isExplicitlyResized(SP<Layout::ITarget> t) const {
        if (!t || m_targets.size() <= 1)
            return false;
        for (const auto& wt : m_explicitlyResized) {
            if (wt.lock() == t)
                return true;
        }
        return false;
    }

    void setTargetColumnWidth(SP<Layout::ITarget> t, float width) {
        auto data = dataFor(t);
        if (data) {
            auto col = data->column.lock();
            if (col) {
                col->setColumnWidth(width);
            }
        }
    }

    void cleanupTargets() {
        std::erase_if(m_targets, [](const auto& wp) { return wp.expired(); });
        std::erase_if(m_explicitlyResized, [](const auto& wp) { return wp.expired(); });
    }

    std::vector<WP<Layout::ITarget>> m_targets;
    std::vector<WP<Layout::ITarget>> m_explicitlyResized;
};
