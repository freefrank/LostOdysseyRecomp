package io.github.freefrank.lostodyssey.probe;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.os.Build;
import android.os.Bundle;
import android.graphics.Color;
import android.graphics.Typeface;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import org.libsdl.app.SDLActivity;

/** Standalone diagnostics. No game files, updater or storage permissions. */
public final class ProbeActivity extends SDLActivity {
    private TextView reportView;
    private String report = "Running native checks…";
    private static native void nativeRequestProbe();
    private static native void nativeRequestTone();

    @Override
    protected String[] getLibraries() {
        return new String[] { "c++_shared", "SDL3", "main" };
    }

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        LinearLayout panel = new LinearLayout(this);
        panel.setOrientation(LinearLayout.VERTICAL);
        panel.setPadding(24, 12, 24, 12);
        ViewCompat.setOnApplyWindowInsetsListener(panel, (view, insets) -> {
            Insets safe = insets.getInsets(WindowInsetsCompat.Type.systemBars()
                    | WindowInsetsCompat.Type.displayCutout());
            view.setPadding(24 + safe.left, 12 + safe.top, 24 + safe.right, 12 + safe.bottom);
            return insets;
        });
        panel.setBackgroundColor(0xDD101820);
        LinearLayout actions = new LinearLayout(this);
        actions.setGravity(Gravity.CENTER_VERTICAL);
        Button rerun = new Button(this);
        rerun.setText("Run checks");
        rerun.setOnClickListener(view -> nativeRequestProbe());
        actions.addView(rerun);
        Button tone = new Button(this);
        tone.setText("Test audio");
        tone.setOnClickListener(view -> nativeRequestTone());
        actions.addView(tone);
        Button copy = new Button(this);
        copy.setText("Copy report");
        copy.setOnClickListener(view -> {
            ClipboardManager clipboard = (ClipboardManager) getSystemService(CLIPBOARD_SERVICE);
            if (clipboard != null) clipboard.setPrimaryClip(ClipData.newPlainText("Android probe", report));
        });
        actions.addView(copy);
        panel.addView(actions);
        reportView = new TextView(this);
        reportView.setTextColor(Color.WHITE);
        reportView.setTypeface(Typeface.MONOSPACE);
        reportView.setTextSize(12);
        reportView.setTextIsSelectable(true);
        reportView.setText(report);
        ScrollView scroll = new ScrollView(this);
        scroll.addView(reportView);
        panel.addView(scroll, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1));
        // Leave the bottom fifth unobscured for the Vulkan clear/present probe.
        addContentView(panel, new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                getResources().getDisplayMetrics().heightPixels * 4 / 5));
        ViewCompat.requestApplyInsets(panel);
    }

    // Called on SDL's native thread; all widget access is marshalled to the UI thread.
    public void showProbeReport(String text) {
        runOnUiThread(() -> {
            report = Build.MANUFACTURER + " " + Build.MODEL + ", Android API " + Build.VERSION.SDK_INT
                    + ", ABI " + String.join(",", Build.SUPPORTED_ABIS) + "\n\n" + text;
            if (reportView != null) reportView.setText(report);
        });
    }
}
