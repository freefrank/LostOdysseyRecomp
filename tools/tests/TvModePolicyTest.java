package io.github.freefrank.lostodyssey;

public final class TvModePolicyTest {
    private static void check(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }
    public static void main(String[] args) {
        check(TvModePolicy.mode(-1) == 0 && TvModePolicy.mode(99) == 0, "invalid persisted mode");
        check(TvModePolicy.height(1) == 1080 && TvModePolicy.width(720) == 1280, "resolution validation");
        check(!TvModePolicy.useDisplay(0, false, true, true), "never choose internal display");
        check(!TvModePolicy.useDisplay(1, true, false, true), "never choose a non-presentation display");
        check(!TvModePolicy.useDisplay(0, true, true, false), "automatic preserves non-wide output");
        check(TvModePolicy.useDisplay(1, true, true, false), "explicit TV fits non-wide output");
        check(TvModePolicy.useDisplay(0, true, true, true), "automatic TV");
        check(!TvModePolicy.useDisplay(2, true, true, true) && !TvModePolicy.useDisplay(3, true, true, true), "mirror and off");
        for (int height : new int[]{720, 1080, 2160}) {
            int width = height * 16 / 9;
            int[] fit = TvModePolicy.fit(width, height);
            check(fit[0] == width && fit[1] == height, "16:9 fills output");
        }
        int[] wide = TvModePolicy.fit(2400, 1080), tall = TvModePolicy.fit(1280, 1024);
        check(wide[0] == 1920 && wide[1] == 1080, "ultrawide fits without crop");
        check(tall[0] == 1280 && tall[1] == 720, "4:3 fits without crop");
        check(!TvModePolicy.wide(2400, 1080) && TvModePolicy.wide(1920, 1080), "phone vs TV aspect");
        check(TvModePolicy.modeScore(1920, 1080, 60, 1080) == 0, "prefer 1080p60");
        check(Double.isInfinite(TvModePolicy.modeScore(3840, 2160, 60, 1080)), "no unsupported signal forcing");
        check(Double.isInfinite(TvModePolicy.modeScore(1920, 1080, Float.NaN, 1080)), "invalid refresh");
        System.out.println("TvModePolicy: display selection, aspect fitting, 720/1080p and 60 Hz PASS");
    }
}
