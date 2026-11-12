package io.github.freefrank.lostodyssey;

/**
 * Auto-hide state of the on-screen CTRL button: after a few idle seconds it
 * slides to the nearest screen edge and leaves a small tab there. Hide-all mode
 * removes the tab too, with a temporary reveal using the same delay. Plain Java
 * (times in milliseconds, positions in pixels) so the JVM tests cover it.
 */
final class TouchCtrlHandle {
    static final long HIDE_DELAY_MS = 4000;
    static final long SLIDE_MS = 200;
    static final int EDGE_TOP = 0, EDGE_BOTTOM = 1, EDGE_LEFT = 2, EDGE_RIGHT = 3;
    /** Share of the button's diameter that stays on screen when retracted. */
    static final float TAB_FRACTION = 1f / 3f;
    static final float TAB_OPACITY = .5f;
    static final float TAB_LABEL_SCALE = .7f;

    private boolean enabled = true, pinned, retracted, hideAll, revealed;
    private long lastActivity, slideStart;
    private float slideFrom, slideTo;

    // Geometry from the last layout() call; fields so onDraw allocates nothing.
    int edge;
    float drawX, drawY, labelX, labelY, labelScale = 1f, labelRotation, opacity = 1f;
    float hitLeft, hitTop, hitRight, hitBottom, clipTop;
    private float topInset;

    /** Restart the timer; hide-all mode waits for an explicit reveal. */
    void reset(long now) {
        revealed = false;
        lastActivity = now;
        retracted = false;
        slideFrom = slideTo = 0f;
        slideStart = now - SLIDE_MS;
    }

    /** A button/tab touch, system bars or Back: bring CTRL back and restart the timer. */
    void activity(long now) {
        revealed = true;
        lastActivity = now;
        slideTo(false, now);
    }

    /** Pinned (editor, CTRL dialog, finger on the button) keeps it fully visible. */
    void setPinned(boolean value, long now) {
        if (pinned == value) return;
        pinned = value;
        lastActivity = now;
        if (value) slideTo(false, now);
    }

    void setEnabled(boolean value, long now) {
        if (enabled == value) return;
        enabled = value;
        lastActivity = now;
        if (!value) slideTo(false, now);
    }

    boolean enabled() { return enabled; }

    void setHideAll(boolean value, long now) {
        if (hideAll == value) return;
        hideAll = value;
        reset(now);
    }

    boolean hideAll() { return hideAll; }

    /** Hidden mode has no tab; only a system-bar/Back reveal or a pinned UI shows CTRL. */
    boolean visible(long now) {
        return !hideAll || pinned || (revealed && now - lastActivity < HIDE_DELAY_MS);
    }

    /** True once the button retracts (including while it is still sliding out). */
    boolean retracted() { return retracted; }

    /** 0 = full button, 1 = retracted tab; starts the slide when the timer expires. */
    float progress(long now) {
        if (hideAll) return 0f;
        if (enabled && !pinned && !retracted && now - lastActivity >= HIDE_DELAY_MS)
            slideTo(true, now);
        return value(now);
    }

    /** 0 = redraw next frame (sliding), positive = wait this long, -1 = nothing pending. */
    long nextWakeMs(long now) {
        if (hideAll) {
            return !pinned && visible(now) ? Math.max(1, lastActivity + HIDE_DELAY_MS - now) : -1;
        }
        if (now - slideStart < SLIDE_MS && slideFrom != slideTo) return 0;
        if (enabled && !pinned && !retracted)
            return Math.max(1, lastActivity + HIDE_DELAY_MS - now);
        return -1;
    }

    private void slideTo(boolean target, long now) {
        if (retracted == target && slideTo == (target ? 1f : 0f)) return;
        slideFrom = value(now);
        slideTo = target ? 1f : 0f;
        slideStart = now;
        retracted = target;
    }

    private float value(long now) {
        float t = (now - slideStart) / (float) SLIDE_MS;
        if (t >= 1f) return slideTo;
        if (t <= 0f) return slideFrom;
        float eased = t * t * (3f - 2f * t);
        return slideFrom + (slideTo - slideFrom) * eased;
    }

    /** The top edge is the line topInset below the view's top (status-bar strip). */
    static int nearestEdge(float cx, float cy, int width, int height, float topInset) {
        int edge = EDGE_TOP;
        float best = cy - topInset;
        if (height - cy < best) { best = height - cy; edge = EDGE_BOTTOM; }
        if (cx < best) { best = cx; edge = EDGE_LEFT; }
        if (width - cx < best) edge = EDGE_RIGHT;
        return edge;
    }

    /**
     * Places the button (centre cx/cy, radius) for the given slide progress and
     * computes the retracted tab's touch rectangle, at least minTouch on each side.
     * A top-edge tab hangs below topInset; draw with clipTop as the clip's top so
     * nothing of it shows in the strip above.
     */
    void layout(float cx, float cy, float radius, int width, int height,
                float minTouch, float topInset, float progress) {
        edge = nearestEdge(cx, cy, width, height, topInset);
        this.topInset = edge == EDGE_TOP ? topInset : 0f;
        clipTop = this.topInset;
        boolean vertical = edge == EDGE_TOP || edge == EDGE_BOTTOM;
        // Distance of the centre from its edge: now, and once retracted.
        float home = edge == EDGE_TOP ? cy - topInset : edge == EDGE_BOTTOM ? height - cy
            : edge == EDGE_LEFT ? cx : width - cx;
        float tabDepth = 2f * radius * TAB_FRACTION;
        float hidden = tabDepth - radius;
        float centre = home + (hidden - home) * progress;
        float label = home + (tabDepth * .5f - home) * progress;
        float along = vertical ? cx : cy;
        drawX = vertical ? cx : fromEdge(centre, width);
        drawY = vertical ? fromEdge(centre, height) : cy;
        labelX = vertical ? cx : fromEdge(label, width);
        labelY = vertical ? fromEdge(label, height) : cy;
        labelScale = 1f + (TAB_LABEL_SCALE - 1f) * progress;
        labelRotation = vertical ? 0f : (edge == EDGE_LEFT ? 90f : -90f) * progress;
        opacity = 1f + (TAB_OPACITY - 1f) * progress;

        float halfLength = Math.max(radius, minTouch * .5f);
        float depth = Math.max(tabDepth, minTouch);
        float near = 0f, far = depth;
        if (vertical) {
            hitLeft = along - halfLength;
            hitRight = along + halfLength;
            hitTop = edge == EDGE_TOP ? topInset + near : height - far;
            hitBottom = edge == EDGE_TOP ? topInset + far : height - near;
        } else {
            hitTop = along - halfLength;
            hitBottom = along + halfLength;
            hitLeft = edge == EDGE_LEFT ? near : width - far;
            hitRight = edge == EDGE_LEFT ? far : width - near;
        }
    }

    private float fromEdge(float distance, int size) {
        if (edge == EDGE_TOP) return topInset + distance;
        return edge == EDGE_LEFT ? distance : size - distance;
    }

    boolean hitTab(float x, float y) {
        return x >= hitLeft && x <= hitRight && y >= hitTop && y <= hitBottom;
    }
}
