package io.github.freefrank.lostodyssey;

/** Android-free selection rules, shared by the game and the diagnostic APK. */
final class TvModePolicy {
    static final int AUTOMATIC = 0, TV_16_9 = 1, ORIGINAL_MIRROR = 2, OFF = 3;
    private TvModePolicy() {}
    static int mode(int value) { return value >= AUTOMATIC && value <= OFF ? value : AUTOMATIC; }
    static int height(int value) { return value == 720 ? 720 : 1080; }
    static int width(int height) { return height(height) * 16 / 9; }
    static boolean wide(int width, int height) {
        if (width <= 0 || height <= 0) return false;
        long a = (long)width * 9, b = (long)height * 16;
        return Math.abs(a - b) * 100 <= b * 2;
    }
    static boolean useDisplay(int mode, boolean external, boolean presentation, boolean wide) {
        return external && presentation && (mode == TV_16_9 || (mode == AUTOMATIC && wide));
    }
    static int[] fit(int width, int height) {
        if (width <= 0 || height <= 0) return new int[]{0, 0};
        if ((long)width * 9 > (long)height * 16) width = (int)((long)height * 16 / 9);
        else height = (int)((long)width * 9 / 16);
        return new int[]{width, height};
    }
    // Both 30 and 60 fps divide a 60 Hz output. Never ask for an unsupported mode.
    static double modeScore(int width, int height, float refresh, int requestedHeight) {
        if (width != width(requestedHeight) || height != height(requestedHeight)
                || !Float.isFinite(refresh) || refresh < 29) return Double.POSITIVE_INFINITY;
        return Math.abs(refresh - 60.0);
    }
}
