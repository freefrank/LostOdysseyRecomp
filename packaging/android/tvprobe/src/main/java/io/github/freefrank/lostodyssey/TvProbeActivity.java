package io.github.freefrank.lostodyssey;

import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.os.Bundle;
import android.view.Choreographer;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import java.io.File;

/** No game files, JNI or Vulkan: checks the SAME public display path as the runtime. */
public final class TvProbeActivity extends Activity {
    private TvDisplayDiagnostics diagnostics;
    private TvOutputController output;
    private PatternSurface pattern;
    private long inputLogTime;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        File external = getExternalFilesDir(null);
        diagnostics = new TvDisplayDiagnostics(this, new File(external == null ? getFilesDir() : external, "logs"));
        diagnostics.start();
        RelativeLayout root = new RelativeLayout(this);
        root.setBackgroundColor(Color.BLACK);
        pattern = new PatternSurface(this);
        root.addView(pattern, new RelativeLayout.LayoutParams(-1, -1));
        LinearLayout buttons = new LinearLayout(this);
        Button settings = new Button(this);
        settings.setText(R.string.tv_mode);
        settings.setOnClickListener(v -> {
            output.wakePhone();
            TvSettingsDialog.show(this, output, diagnostics);
        });
        buttons.addView(settings);
        Button rate = new Button(this);
        rate.setText("30 / 60 FPS");
        rate.setOnClickListener(v -> {
            pattern.fps = pattern.fps == 30 ? 60 : 30;
            diagnostics.event("pattern requested-fps=" + pattern.fps + " (not game/Vulkan performance)");
        });
        buttons.addView(rate);
        RelativeLayout.LayoutParams bar = new RelativeLayout.LayoutParams(-2, -2);
        bar.addRule(RelativeLayout.ALIGN_PARENT_BOTTOM);
        root.addView(buttons, bar);
        setContentView(root);
        output = new TvOutputController(this, root, pattern, diagnostics,
            (externalOutput, height) -> diagnostics.event("probe output=" + externalOutput + " requested-height=" + height));
        output.start();
    }
    @Override protected void onResume() {
        super.onResume();
        if (output != null) output.resume();
        if (pattern != null) pattern.start();
    }
    @Override protected void onPause() {
        if (pattern != null) pattern.stop();
        if (output != null) output.pause();
        super.onPause();
    }
    @Override protected void onDestroy() {
        if (output != null) output.stop();
        if (diagnostics != null) diagnostics.stop();
        super.onDestroy();
    }
    @Override public boolean dispatchKeyEvent(KeyEvent event) {
        if (diagnostics != null) diagnostics.event("probe key device=" + event.getDeviceId()
            + " code=" + event.getKeyCode() + " action=" + event.getAction() + " source=" + event.getSource());
        if (KeyEvent.isGamepadButton(event.getKeyCode())) {
            pattern.lastInput = "Button " + event.getKeyCode();
            if (event.getAction() == KeyEvent.ACTION_DOWN && event.getKeyCode() == KeyEvent.KEYCODE_BUTTON_START) {
                output.wakePhone();
                TvSettingsDialog.show(this, output, diagnostics);
            }
            return true;
        }
        return super.dispatchKeyEvent(event);
    }
    @Override public boolean dispatchGenericMotionEvent(MotionEvent event) {
        if (event.isFromSource(android.view.InputDevice.SOURCE_JOYSTICK)) {
            pattern.lastInput = "Stick X=" + event.getAxisValue(MotionEvent.AXIS_X) + " Y=" + event.getAxisValue(MotionEvent.AXIS_Y);
            long now = android.os.SystemClock.uptimeMillis();
            if (now - inputLogTime > 500) {
                inputLogTime = now;
                diagnostics.event("probe joystick device=" + event.getDeviceId() + " " + pattern.lastInput);
            }
            return true;
        }
        return super.dispatchGenericMotionEvent(event);
    }

    private final class PatternSurface extends SurfaceView implements SurfaceHolder.Callback, Choreographer.FrameCallback {
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private boolean ready, running;
        private long previous, frames, reportAt;
        int fps = 30;
        String lastInput = "Controller: press a button or move a stick";
        PatternSurface(Context context) { super(context); getHolder().addCallback(this); }
        void start() { running = true; previous = 0; Choreographer.getInstance().removeFrameCallback(this); Choreographer.getInstance().postFrameCallback(this); }
        void stop() { running = false; Choreographer.getInstance().removeFrameCallback(this); }
        @Override public void surfaceCreated(SurfaceHolder holder) { ready = true; }
        @Override public void surfaceDestroyed(SurfaceHolder holder) {
            ready = false;
            diagnostics.surface("probe-destroyed", holder, 0, 0, getDisplay());
        }
        @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
            ready = true;
            diagnostics.surface("probe-changed", holder, width, height, getDisplay());
            if (output != null) output.surfaceReady();
        }
        @Override public void doFrame(long time) {
            if (!running) return;
            Choreographer.getInstance().postFrameCallback(this);
            if (!ready || (previous != 0 && time - previous < 1_000_000_000L / fps - 1_000_000L)) return;
            previous = time;
            Canvas canvas = null;
            try {
                canvas = getHolder().lockCanvas();
                if (canvas == null) return;
                drawPattern(canvas, time);
                ++frames;
            } catch (IllegalArgumentException | IllegalStateException e) {
                diagnostics.event("probe canvas unavailable: " + e);
            } finally {
                if (canvas != null) getHolder().unlockCanvasAndPost(canvas);
            }
            if (reportAt == 0) reportAt = time;
            if (time - reportAt >= 5_000_000_000L) {
                diagnostics.event("probe canvas-post rate=" + frames * 1e9 / (time - reportAt)
                    + " requested=" + fps + "; NOT measured HDMI scanout or Vulkan game FPS");
                frames = 0; reportAt = time;
            }
        }
        private void drawPattern(Canvas canvas, long time) {
            int w = canvas.getWidth(), h = canvas.getHeight();
            canvas.drawColor(Color.rgb(24, 32, 48));
            paint.setStrokeWidth(Math.max(2, h / 180f));
            paint.setColor(Color.WHITE);
            paint.setStyle(Paint.Style.STROKE);
            canvas.drawRect(4, 4, w - 4, h - 4, paint);
            for (int i = 1; i < 16; ++i) canvas.drawLine(w * i / 16f, 0, w * i / 16f, h, paint);
            for (int i = 1; i < 9; ++i) canvas.drawLine(0, h * i / 9f, w, h * i / 9f, paint);
            paint.setColor(Color.YELLOW);
            canvas.drawCircle(w / 2f, h / 2f, h * .25f, paint);
            paint.setStyle(Paint.Style.FILL);
            paint.setTextSize(h / 24f);
            canvas.drawText(w + " x " + h + " | " + fps + " FPS pattern | no Vulkan/game", w / 20f, h / 5f, paint);
            canvas.drawText(lastInput, w / 20f, h * .8f, paint);
            paint.setColor(Color.GREEN);
            float x = (time % 3_000_000_000L) / 3_000_000_000f * w;
            canvas.drawRect(x, h * .45f, x + h / 20f, h * .55f, paint);
            paint.setTextSize(h / 12f);
            canvas.drawText("1", 12, h / 12f, paint);
            canvas.drawText("2", w - h / 12f, h / 12f, paint);
            canvas.drawText("3", 12, h - 12, paint);
            canvas.drawText("4", w - h / 12f, h - 12, paint);
        }
    }
}
