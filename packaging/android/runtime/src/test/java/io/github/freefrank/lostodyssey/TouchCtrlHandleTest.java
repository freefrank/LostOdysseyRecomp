package io.github.freefrank.lostodyssey;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

public class TouchCtrlHandleTest {
    private static final long T0 = 10_000;
    private static final int W = 2560, H = 1600;

    private static TouchCtrlHandle fresh() {
        TouchCtrlHandle handle = new TouchCtrlHandle();
        handle.reset(T0);
        return handle;
    }

    @Test
    public void hideAllHasNoButtonOrTabUntilRevealed() {
        TouchCtrlHandle handle = fresh();
        handle.setHideAll(true, T0);
        assertTrue(handle.hideAll());
        assertFalse(handle.visible(T0));
        assertEquals(0f, handle.progress(T0 + 60_000), 0f);
        assertFalse(handle.retracted());
        assertEquals(-1, handle.nextWakeMs(T0 + 60_000));
    }

    @Test
    public void hiddenRevealExpiresEvenWhenAutoHideIsDisabled() {
        TouchCtrlHandle handle = fresh();
        handle.setEnabled(false, T0);
        handle.setHideAll(true, T0);
        long reveal = T0 + 1000;
        handle.activity(reveal);
        assertTrue(handle.visible(reveal));
        assertEquals(TouchCtrlHandle.HIDE_DELAY_MS, handle.nextWakeMs(reveal));
        assertTrue(handle.visible(reveal + TouchCtrlHandle.HIDE_DELAY_MS - 1));
        assertFalse(handle.visible(reveal + TouchCtrlHandle.HIDE_DELAY_MS));
        assertEquals(-1, handle.nextWakeMs(reveal + TouchCtrlHandle.HIDE_DELAY_MS));
        assertEquals(0f, handle.progress(reveal + TouchCtrlHandle.HIDE_DELAY_MS), 0f);
        assertFalse(handle.retracted());
    }

    @Test
    public void repeatedRevealAndDialogPinRestartHiddenTimer() {
        TouchCtrlHandle handle = fresh();
        handle.setHideAll(true, T0);
        handle.activity(T0 + 1000);
        handle.activity(T0 + 4000);
        assertTrue(handle.visible(T0 + 7000));
        handle.setPinned(true, T0 + 7000);
        assertTrue(handle.visible(T0 + 60_000));
        assertEquals(-1, handle.nextWakeMs(T0 + 60_000));
        handle.setPinned(false, T0 + 60_000);
        assertTrue(handle.visible(T0 + 63_999));
        assertFalse(handle.visible(T0 + 64_000));
    }

    @Test
    public void resetKeepsHiddenPreferenceAndTurningItOffRestoresAutoHide() {
        TouchCtrlHandle handle = fresh();
        handle.setHideAll(true, T0);
        handle.activity(T0);
        handle.reset(T0 + 1000);
        assertTrue(handle.hideAll());
        assertFalse(handle.visible(T0 + 1000));
        handle.setHideAll(false, T0 + 2000);
        assertTrue(handle.visible(T0 + 2000));
        handle.progress(T0 + 6000);
        assertTrue(handle.retracted());
        assertTrue(handle.visible(T0 + 60_000));
    }

    @Test
    public void retractsAfterIdleDelayAndSlides() {
        TouchCtrlHandle handle = fresh();
        assertEquals(0f, handle.progress(T0 + 3999), 0f);
        assertEquals(1, handle.nextWakeMs(T0 + 3999));
        long start = T0 + TouchCtrlHandle.HIDE_DELAY_MS;
        assertEquals(0f, handle.progress(start), 0f);
        assertTrue(handle.retracted());
        assertEquals(0, handle.nextWakeMs(start + 100));
        assertEquals(.5f, handle.progress(start + 100), 1e-4f);
        assertEquals(1f, handle.progress(start + TouchCtrlHandle.SLIDE_MS), 0f);
        assertEquals(-1, handle.nextWakeMs(start + TouchCtrlHandle.SLIDE_MS));
    }

    @Test
    public void activityRestoresAndRestartsTimer() {
        TouchCtrlHandle handle = fresh();
        handle.progress(T0 + 4000);
        long hidden = T0 + 4000 + TouchCtrlHandle.SLIDE_MS;
        assertEquals(1f, handle.progress(hidden), 0f);
        handle.activity(hidden);
        assertFalse(handle.retracted());
        assertEquals(0f, handle.progress(hidden + TouchCtrlHandle.SLIDE_MS), 0f);
        assertEquals(0f, handle.progress(hidden + 3999), 0f);
        handle.progress(hidden + 4000);
        assertTrue(handle.retracted());
    }

    @Test
    public void pinnedAndDisabledStayVisible() {
        TouchCtrlHandle handle = fresh();
        handle.setPinned(true, T0);
        assertEquals(0f, handle.progress(T0 + 60_000), 0f);
        assertEquals(-1, handle.nextWakeMs(T0 + 60_000));
        handle.setPinned(false, T0 + 60_000);
        assertEquals(0f, handle.progress(T0 + 63_999), 0f);
        handle.progress(T0 + 64_000);
        assertTrue(handle.retracted());

        TouchCtrlHandle off = fresh();
        off.setEnabled(false, T0);
        assertEquals(0f, off.progress(T0 + 60_000), 0f);
        assertFalse(off.retracted());
    }

    @Test
    public void picksNearestEdgeMeasuringTopFromTheSafeStrip() {
        assertEquals(TouchCtrlHandle.EDGE_TOP, TouchCtrlHandle.nearestEdge(1280, 178, W, H, 96));
        assertEquals(TouchCtrlHandle.EDGE_BOTTOM, TouchCtrlHandle.nearestEdge(1280, 1500, W, H, 96));
        assertEquals(TouchCtrlHandle.EDGE_LEFT, TouchCtrlHandle.nearestEdge(50, 800, W, H, 96));
        assertEquals(TouchCtrlHandle.EDGE_RIGHT, TouchCtrlHandle.nearestEdge(2500, 800, W, H, 96));
        // 150 px from the left, 200 px from the top: the top edge wins once it
        // is measured from the 96 px strip.
        assertEquals(TouchCtrlHandle.EDGE_LEFT, TouchCtrlHandle.nearestEdge(150, 200, W, H, 0));
        assertEquals(TouchCtrlHandle.EDGE_TOP, TouchCtrlHandle.nearestEdge(150, 200, W, H, 96));
    }

    @Test
    public void topTabHangsBelowTheSafeStrip() {
        TouchCtrlHandle handle = new TouchCtrlHandle();
        // Radius 90: a third of the diameter (60 px) stays visible below y = 96.
        handle.layout(1280, 200, 90, W, H, 48, 96, 1f);
        assertEquals(TouchCtrlHandle.EDGE_TOP, handle.edge);
        assertEquals(96f, handle.clipTop, 0f);
        assertEquals(96f - 30f, handle.drawY, 1e-3f);
        assertEquals(96f + 30f, handle.labelY, 1e-3f);
        assertEquals(TouchCtrlHandle.TAB_OPACITY, handle.opacity, 0f);
        assertEquals(96f, handle.hitTop, 0f);
        assertEquals(156f, handle.hitBottom, 0f);
        assertEquals(1190f, handle.hitLeft, 0f);
        assertEquals(1370f, handle.hitRight, 0f);
        assertTrue(handle.hitTab(1280, 100));
        assertFalse(handle.hitTab(1280, 50));

        // Unretracted, the button is where the layout put it.
        handle.layout(1280, 200, 90, W, H, 48, 96, 0f);
        assertEquals(200f, handle.drawY, 0f);
        assertEquals(1f, handle.opacity, 0f);
    }

    @Test
    public void sideAndBottomTabsHugTheirEdge() {
        TouchCtrlHandle handle = new TouchCtrlHandle();
        handle.layout(150, 800, 90, W, H, 48, 96, 1f);
        assertEquals(TouchCtrlHandle.EDGE_LEFT, handle.edge);
        assertEquals(0f, handle.clipTop, 0f);
        assertEquals(-30f, handle.drawX, 1e-3f);
        assertEquals(800f, handle.drawY, 0f);
        assertEquals(30f, handle.labelX, 1e-3f);
        assertEquals(90f, handle.labelRotation, 0f);
        assertEquals(0f, handle.hitLeft, 0f);
        assertEquals(60f, handle.hitRight, 0f);
        assertEquals(710f, handle.hitTop, 0f);
        assertEquals(890f, handle.hitBottom, 0f);

        handle.layout(2400, 800, 90, W, H, 48, 96, 1f);
        assertEquals(TouchCtrlHandle.EDGE_RIGHT, handle.edge);
        assertEquals(W + 30f, handle.drawX, 1e-3f);
        assertEquals(-90f, handle.labelRotation, 0f);
        assertEquals(W - 60f, handle.hitLeft, 0f);
        assertEquals(W, handle.hitRight, 0f);

        handle.layout(1280, 1450, 90, W, H, 48, 96, 1f);
        assertEquals(TouchCtrlHandle.EDGE_BOTTOM, handle.edge);
        assertEquals(H + 30f, handle.drawY, 1e-3f);
        assertEquals(H - 30f, handle.labelY, 1e-3f);
        assertEquals(H - 60f, handle.hitTop, 0f);
        assertEquals(H, handle.hitBottom, 0f);
    }

    @Test
    public void smallTabStillGetsMinimumTouchTarget() {
        TouchCtrlHandle handle = new TouchCtrlHandle();
        // Radius 30 draws a 20 px tab; 48 dp is 126 px here.
        handle.layout(1280, 200, 30, W, H, 126, 96, 1f);
        assertEquals(126f, handle.hitBottom - handle.hitTop, 0f);
        assertEquals(126f, handle.hitRight - handle.hitLeft, 0f);
        handle.layout(100, 800, 30, W, H, 126, 96, 1f);
        assertEquals(126f, handle.hitRight - handle.hitLeft, 0f);
        assertEquals(126f, handle.hitBottom - handle.hitTop, 0f);
    }
}
