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
        if (target) {
            markUserModified(target);
        }
        Layout::Tiled::CScrollingAlgorithm::resizeTarget(Δ, target, corner);
    }

    // Intercept keyboard/command colresize
    virtual Config::ErrorResult layoutMsg(const std::string_view& sv) override {
        if (sv.starts_with("colresize +") || sv.starts_with("colresize -") || sv.starts_with("colresize 0.")) {
            auto focused = Desktop::focusState()->window();
            if (focused && focused->layoutTarget()) {
                markUserModified(focused->layoutTarget());
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
        std::erase_if(m_userModified, [target](const auto& wp) {
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

        applyDynamicResizing(nullptr);
    }

    void applyDynamicResizing(SP<Layout::ITarget> newTargetPtr) {
        cleanupTargets();
        const size_t count = m_targets.size();

        if (count == 1) {
            auto t = m_targets[0].lock();
            // Sizes that were modified by the user MUST stay.
            if (t && !isUserModified(t)) {
                setTargetColumnWidth(t, 1.0f);
            }
        } else if (count == 2) {
            for (const auto& wt : m_targets) {
                auto t = wt.lock();
                // Sizes that were modified by the user MUST stay.
                if (t && !isUserModified(t)) {
                    setTargetColumnWidth(t, 0.5f);
                }
            }
        } else if (count >= 3) {
            // New window gets default 1/3 width
            if (newTargetPtr && !isUserModified(newTargetPtr)) {
                setTargetColumnWidth(newTargetPtr, 0.333333f);
            }
            // For other windows, only transition from 0.5 (2-window state) if NOT modified by user
            for (const auto& wt : m_targets) {
                auto t = wt.lock();
                if (t && t != newTargetPtr && !isUserModified(t)) {
                    auto data = dataFor(t);
                    if (data) {
                        auto col = data->column.lock();
                        // Only change if it was on the default 0.5 from the 2-window state
                        if (col && std::abs(col->getColumnWidth() - 0.5f) < 0.01f) {
                            col->setColumnWidth(0.333333f);
                        }
                    }
                }
            }
        }

        recalculate();
        (void)Layout::Tiled::CScrollingAlgorithm::layoutMsg("fit_into_view");
    }

  private:
    void markUserModified(SP<Layout::ITarget> t) {
        if (!isUserModified(t)) {
            m_userModified.push_back(t);
        }
    }

    bool isUserModified(SP<Layout::ITarget> t) const {
        if (!t)
            return false;

        // 1. Explicitly marked via colresize or drag
        for (const auto& wt : m_userModified) {
            if (wt.lock() == t)
                return true;
        }

        // 2. Also check if the column width deviates from default standard widths (1.0, 0.5, 0.333)
        auto data = dataFor(t);
        if (data) {
            auto col = data->column.lock();
            if (col) {
                float w = col->getColumnWidth();
                bool isDefault1 = std::abs(w - 1.0f) < 0.01f;
                bool isDefault2 = std::abs(w - 0.5f) < 0.01f;
                bool isDefault3 = std::abs(w - 0.333333f) < 0.01f;
                if (!isDefault1 && !isDefault2 && !isDefault3) {
                    return true;
                }
            }
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
        std::erase_if(m_userModified, [](const auto& wp) { return wp.expired(); });
    }

    std::vector<WP<Layout::ITarget>> m_targets;
    std::vector<WP<Layout::ITarget>> m_userModified;
};
