package io.github.freefrank.lostodyssey;

import android.app.AlertDialog;
import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.hardware.input.InputManager;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.SparseArray;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.TextView;

/** Local touch controls layered over SDL's surface; physical pads stay in SDL. */
final class TouchControlsView extends View {
    private static final long CONTROLLER_POLL_MS = 1000;
    private static final int UP = 0x0001, DOWN = 0x0002, LEFT = 0x0004, RIGHT = 0x0008;
    private static final int START = 0x0010, BACK = 0x0020;
    private static final int L3 = 0x0040, R3 = 0x0080;
    private static final int LB = 0x0100, RB = 0x0200;
    private static final int A = 0x1000, B = 0x2000, X = 0x4000, Y = 0x8000;
    private static final String[] NAMES = {
        "LEFT STICK", "RIGHT STICK", "D-PAD", "A", "B", "X", "Y",
        "L3", "R3", "LB", "LT", "BACK", "START", "RT", "RB"
    };

    private static final class Pointer {
        final int element;
        float x, y;

        Pointer(int element, float x, float y) {
            this.element = element;
            this.x = x;
            this.y = y;
        }
    }

    private final RuntimeActivity activity;
    private final SharedPreferences preferences;
    private final InputManager inputManager;
    private final TouchControllerVisibility visibility;
    private final Handler controllerHandler = new Handler(Looper.getMainLooper());
    private final Runnable controllerPoll = new Runnable() {
        @Override public void run() {
            if (!hostResumed || !isAttachedToWindow()) return;
            refreshControllers();
            controllerHandler.postDelayed(this, CONTROLLER_POLL_MS);
        }
    };
    private final InputManager.InputDeviceListener deviceListener = new InputManager.InputDeviceListener() {
        @Override public void onInputDeviceAdded(int deviceId) { refreshControllers(); }
        @Override public void onInputDeviceRemoved(int deviceId) { refreshControllers(); }
        @Override public void onInputDeviceChanged(int deviceId) { refreshControllers(); }
    };
    private final TouchCtrlHandle ctrl = new TouchCtrlHandle();
    private final Runnable ctrlWake = this::invalidate;
    private long ctrlWakeAt = -1;
    private boolean settingsDialogOpen, settingsPointerRestores;
    private final SparseArray<Pointer> pointers = new SparseArray<>();
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private TouchControlLayout layout, draft;
    private CheckBox settingsToggle;
    private TextView controllerHint;
    private boolean settingsChoiceEdited, syncingSettingsToggle;
    private boolean editing, customLayout, hostResumed, deviceListenerRegistered;
    private int settingsPointer = -1, dragPointer = -1, toolbarPointer = -1;
    private int selected = -1, toolbarPressed = -1;
    private float dragOffsetX, dragOffsetY, baseUnit;
    private int shownButtons, shownLt, shownRt, shownLx, shownLy, shownRx, shownRy;

    TouchControlsView(RuntimeActivity activity) {
        super(activity);
        this.activity = activity;
        preferences = activity.getSharedPreferences("touch_controls", Context.MODE_PRIVATE);
        visibility = new TouchControllerVisibility(preferences.getBoolean("enabled", true));
        inputManager = (InputManager) activity.getSystemService(Context.INPUT_SERVICE);
        customLayout = preferences.getInt("layout_version", 0) == 1;
        long now = SystemClock.uptimeMillis();
        ctrl.reset(now);
        ctrl.setEnabled(preferences.getBoolean("ctrl_auto_hide", true), now);
        ctrl.setHideAll(preferences.getBoolean("hide_all", false), now);
        setClickable(true);
        setContentDescription("Touch controller settings");
    }

    private void refreshControllers() {
        int controllers = 0;
        if (inputManager != null) {
            for (int deviceId : inputManager.getInputDeviceIds()) {
                InputDevice device = inputManager.getInputDevice(deviceId);
                if (device == null || device.isVirtual()) continue;
                int sources = device.getSources();
                if ((sources & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD ||
                    (sources & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK) {
                    ++controllers;
                }
            }
        }
        if (RuntimeActivity.nativeHasConnectedController()) controllers = Math.max(controllers, 1);
        if (!visibility.setControllerCount(controllers)) return;
        clearTouches();
        if (settingsToggle != null && !settingsChoiceEdited) {
            syncingSettingsToggle = true;
            settingsToggle.setChecked(visibility.visible());
            syncingSettingsToggle = false;
        }
        if (controllerHint != null)
            controllerHint.setVisibility(visibility.controllerPresent() ? VISIBLE : GONE);
        invalidate();
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        startControllerWatch();
    }

    private void startControllerWatch() {
        if (!hostResumed || !isAttachedToWindow()) return;
        if (inputManager != null && !deviceListenerRegistered) {
            inputManager.registerInputDeviceListener(deviceListener, controllerHandler);
            deviceListenerRegistered = true;
        }
        refreshControllers();
        controllerHandler.removeCallbacks(controllerPoll);
        controllerHandler.postDelayed(controllerPoll, CONTROLLER_POLL_MS);
    }

    private void stopControllerWatch() {
        controllerHandler.removeCallbacks(controllerPoll);
        if (inputManager != null && deviceListenerRegistered) {
            inputManager.unregisterInputDeviceListener(deviceListener);
            deviceListenerRegistered = false;
        }
    }

    void onHostResume() {
        hostResumed = true;
        // Back from a CTRL page or another app: show CTRL again, retract later.
        ctrl.reset(SystemClock.uptimeMillis());
        invalidate();
        startControllerWatch();
    }

    void onHostPause() {
        hostResumed = false;
        stopControllerWatch();
        cancelCtrlWake();
        clearTouches();
    }

    /** System bars and Android Back restore only CTRL, never the hidden gamepad. */
    void revealCtrl() {
        ctrl.activity(SystemClock.uptimeMillis());
        invalidate();
    }

    private boolean controlsVisible() {
        return !ctrl.hideAll() && visibility.visible();
    }

    boolean hidesAllControls() { return ctrl.hideAll(); }

    private static float bounded(float value, float minimum, float maximum,
                                 float fallback) {
        return Float.isFinite(value) ? Math.max(minimum, Math.min(maximum, value))
            : fallback;
    }

    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);
        if (width <= 0 || height <= 0) return;
        baseUnit = Math.min(width * .0715f, height * .1625f);
        float size = bounded(preferences.getFloat("size", 1f),
                             TouchControlLayout.MIN_SIZE,
                             TouchControlLayout.MAX_SIZE, 1f);
        float opacity = bounded(preferences.getFloat("opacity", 1f),
                                TouchControlLayout.MIN_OPACITY,
                                TouchControlLayout.MAX_OPACITY, 1f);
        TouchControlLayout fresh = TouchControlLayout.defaults(
            width, height, baseUnit * size);
        fresh.size = size;
        fresh.opacity = opacity;
        if (customLayout) {
            for (int id = 0; id < TouchControlLayout.COUNT; ++id) {
                float storedX = preferences.getFloat("x_" + id, fresh.x[id]);
                float storedY = preferences.getFloat("y_" + id, fresh.y[id]);
                if (Float.isFinite(storedX)) fresh.x[id] = storedX;
                if (Float.isFinite(storedY)) fresh.y[id] = storedY;
                fresh.visible[id] = preferences.getBoolean("visible_" + id, true);
            }
            float ctrlX = preferences.getFloat("ctrl_x", fresh.ctrlX);
            float ctrlY = preferences.getFloat("ctrl_y", fresh.ctrlY);
            if (Float.isFinite(ctrlX)) fresh.ctrlX = ctrlX;
            if (Float.isFinite(ctrlY)) fresh.ctrlY = ctrlY;
        }
        layout = fresh;
        layout.clampAll(width, height, baseUnit * layout.size, 0f);
        if (draft != null) draft.clampAll(width, height, baseUnit * draft.size,
                                          editorReservedBottom());
        clearTouches();
    }

    private float editorReservedBottom() { return getHeight() * .15f; }
    private TouchControlLayout activeLayout() { return editing ? draft : layout; }
    private float unit() { return baseUnit * activeLayout().size; }
    private float px(int id) { return activeLayout().pixelX(id, getWidth()); }
    private float py(int id) { return activeLayout().pixelY(id, getHeight()); }
    private float settingX() { return activeLayout().ctrlX * getWidth(); }
    private float settingY() { return activeLayout().ctrlY * getHeight(); }
    private float settingRadius() { return unit() * .45f; }
    private boolean onCtrl(float x, float y) {
        return near(x, y, settingX(), settingY(), unit() * TouchControlLayout.CTRL_HIT_RADIUS);
    }

    /** Lays out CTRL for the current slide progress (see TouchCtrlHandle). */
    private float layoutCtrl(long now) {
        float progress = ctrl.progress(now);
        // The retracted tab sits at the physical screen edge: the system bars are
        // hidden in the game, so the layout's status-bar strip is not reserved here.
        ctrl.layout(settingX(), settingY(), settingRadius(), getWidth(), getHeight(),
                    48f * getResources().getDisplayMetrics().density, 0f, progress);
        return progress;
    }

    private void syncCtrlPin() {
        ctrl.setPinned(editing || settingsDialogOpen || settingsPointer != -1,
                       SystemClock.uptimeMillis());
        invalidate();
    }

    private void cancelCtrlWake() {
        controllerHandler.removeCallbacks(ctrlWake);
        ctrlWakeAt = -1;
    }

    private void scheduleCtrlWake(long now) {
        long wait = ctrl.nextWakeMs(now);
        if (wait == 0) {
            postInvalidateOnAnimation();
        } else if (wait > 0 && hostResumed) {
            long at = now + wait;
            if (at == ctrlWakeAt) return;
            controllerHandler.removeCallbacks(ctrlWake);
            controllerHandler.postAtTime(ctrlWake, at);
            ctrlWakeAt = at;
        }
    }

    private static boolean near(float x, float y, float cx, float cy, float radius) {
        float dx = x - cx, dy = y - cy;
        return dx * dx + dy * dy <= radius * radius;
    }

    private boolean hit(int id, float x, float y) {
        float radius = TouchControlLayout.hitRadius(id) * unit();
        return near(x, y, px(id), py(id), radius);
    }

    private int elementAt(float x, float y, boolean includeHidden) {
        int best = -1;
        float bestDistance = Float.POSITIVE_INFINITY;
        for (int id = 0; id < TouchControlLayout.COUNT; ++id) {
            if (!includeHidden && !activeLayout().visible[id]) continue;
            float radius = TouchControlLayout.hitRadius(id) * unit();
            float dx = x - px(id), dy = y - py(id);
            float distance = (dx * dx + dy * dy) / (radius * radius);
            if (distance <= 1f && distance < bestDistance) {
                best = id;
                bestDistance = distance;
            }
        }
        return best;
    }

    private static int stick(float value) {
        if (Math.abs(value) < .12f) return 0;
        return Math.round(Math.max(-1f, Math.min(1f, value)) * 32767f);
    }

    private static int buttonMask(int element) {
        switch (element) {
            case TouchControlLayout.A: return A;
            case TouchControlLayout.B: return B;
            case TouchControlLayout.X: return X;
            case TouchControlLayout.Y: return Y;
            case TouchControlLayout.L3: return L3;
            case TouchControlLayout.R3: return R3;
            case TouchControlLayout.LB: return LB;
            case TouchControlLayout.RB: return RB;
            case TouchControlLayout.BACK: return BACK;
            case TouchControlLayout.START: return START;
            default: return 0;
        }
    }

    private void publish() {
        int buttons = 0, lt = 0, rt = 0, lx = 0, ly = 0, rx = 0, ry = 0;
        boolean leftAssigned = false, rightAssigned = false;
        if (controlsVisible() && !editing && layout != null && baseUnit > 0f) {
            float unit = unit();
            for (int i = 0; i < pointers.size(); ++i) {
                Pointer pointer = pointers.valueAt(i);
                int id = pointer.element;
                if (!layout.visible[id]) continue;
                if (id == TouchControlLayout.LEFT_STICK && !leftAssigned) {
                    float dx = (pointer.x - px(id)) / unit;
                    float dy = (py(id) - pointer.y) / unit;
                    float length = (float) Math.hypot(dx, dy);
                    if (length > 1f) { dx /= length; dy /= length; }
                    lx = stick(dx); ly = stick(dy); leftAssigned = true;
                } else if (id == TouchControlLayout.RIGHT_STICK && !rightAssigned) {
                    float dx = (pointer.x - px(id)) / unit;
                    float dy = (py(id) - pointer.y) / unit;
                    float length = (float) Math.hypot(dx, dy);
                    if (length > 1f) { dx /= length; dy /= length; }
                    rx = stick(dx); ry = stick(dy); rightAssigned = true;
                } else if (id == TouchControlLayout.DPAD) {
                    float dx = (pointer.x - px(id)) / unit;
                    float dy = (pointer.y - py(id)) / unit;
                    if (dx < -.28f) buttons |= LEFT;
                    if (dx > .28f) buttons |= RIGHT;
                    if (dy < -.28f) buttons |= UP;
                    if (dy > .28f) buttons |= DOWN;
                } else if (hit(id, pointer.x, pointer.y)) {
                    if (id == TouchControlLayout.LT) lt = 255;
                    else if (id == TouchControlLayout.RT) rt = 255;
                    else buttons |= buttonMask(id);
                }
            }
        }
        shownButtons = buttons; shownLt = lt; shownRt = rt;
        shownLx = lx; shownLy = ly; shownRx = rx; shownRy = ry;
        RuntimeActivity.nativeSetTouchInput(buttons, lt, rt, lx, ly, rx, ry);
        invalidate();
    }

    void clearTouches() {
        pointers.clear();
        boolean heldCtrl = settingsPointer != -1;
        settingsPointer = dragPointer = toolbarPointer = -1;
        settingsPointerRestores = false;
        if (heldCtrl) syncCtrlPin();
        toolbarPressed = -1;
        publish();
    }

    private void setControlsEnabled(boolean value) {
        clearTouches();
        boolean previous = visibility.enabledPreference();
        visibility.applyShow(value);
        if (visibility.enabledPreference() != previous)
            preferences.edit().putBoolean("enabled", visibility.enabledPreference()).apply();
        invalidate();
    }

    private void saveLayout() {
        SharedPreferences.Editor editor = preferences.edit().putInt("layout_version", 1);
        for (int id = 0; id < TouchControlLayout.COUNT; ++id) {
            editor.putFloat("x_" + id, layout.x[id]);
            editor.putFloat("y_" + id, layout.y[id]);
            editor.putBoolean("visible_" + id, layout.visible[id]);
        }
        editor.putFloat("ctrl_x", layout.ctrlX).putFloat("ctrl_y", layout.ctrlY);
        editor.apply();
        customLayout = true;
    }

    private void applySettings(boolean show, int sizePercent, int opacityPercent,
                               boolean ctrlAutoHide, boolean hideAll) {
        clearTouches();
        ctrl.setEnabled(ctrlAutoHide, SystemClock.uptimeMillis());
        ctrl.setHideAll(hideAll, SystemClock.uptimeMillis());
        activity.updateSystemBarsBehavior();
        float size = sizePercent / 100f;
        if (!customLayout) layout = TouchControlLayout.defaults(
            getWidth(), getHeight(), baseUnit * size);
        layout.size = size;
        layout.opacity = opacityPercent / 100f;
        layout.clampAll(getWidth(), getHeight(), baseUnit * layout.size, 0f);
        preferences.edit().putFloat("size", layout.size)
            .putFloat("opacity", layout.opacity)
            .putBoolean("ctrl_auto_hide", ctrlAutoHide)
            .putBoolean("hide_all", hideAll).apply();
        setControlsEnabled(show);
    }

    private void showSettings() {
        clearTouches();
        Context strings = SaveLocationStrings.context(activity);
        LinearLayout content = new LinearLayout(activity);
        content.setOrientation(LinearLayout.VERTICAL);
        int padding = Math.round(20f * getResources().getDisplayMetrics().density);
        content.setPadding(padding, 0, padding, 0);

        CheckBox toggle = new CheckBox(activity);
        toggle.setText(strings.getString(R.string.touch_show_controls));
        toggle.setChecked(visibility.visible());
        settingsChoiceEdited = false;
        toggle.setOnCheckedChangeListener((button, checked) -> {
            if (!syncingSettingsToggle) settingsChoiceEdited = true;
        });
        content.addView(toggle);
        TextView hint = new TextView(activity);
        hint.setText(strings.getString(R.string.touch_controller_hint));
        hint.setVisibility(visibility.controllerPresent() ? VISIBLE : GONE);
        content.addView(hint);
        settingsToggle = toggle;
        controllerHint = hint;

        TextView sizeLabel = new TextView(activity);
        SeekBar size = new SeekBar(activity);
        size.setMax(80);
        size.setProgress(Math.round(layout.size * 100f) - 60);
        sizeLabel.setText(strings.getString(R.string.touch_control_size, size.getProgress() + 60));
        size.setOnSeekBarChangeListener(seekListener(value ->
            sizeLabel.setText(strings.getString(R.string.touch_control_size, value + 60))));
        content.addView(sizeLabel);
        content.addView(size);

        TextView opacityLabel = new TextView(activity);
        SeekBar opacity = new SeekBar(activity);
        opacity.setMax(75);
        opacity.setProgress(Math.round(layout.opacity * 100f) - 25);
        opacityLabel.setText(strings.getString(R.string.touch_opacity, opacity.getProgress() + 25));
        opacity.setOnSeekBarChangeListener(seekListener(value ->
            opacityLabel.setText(strings.getString(R.string.touch_opacity, value + 25))));
        content.addView(opacityLabel);
        content.addView(opacity);

        CheckBox ctrlAutoHide = new CheckBox(activity);
        ctrlAutoHide.setText(strings.getString(R.string.touch_ctrl_auto_hide));
        ctrlAutoHide.setChecked(ctrl.enabled());
        content.addView(ctrlAutoHide);

        CheckBox hideAll = new CheckBox(activity);
        hideAll.setText(strings.getString(R.string.touch_hide_all));
        hideAll.setChecked(ctrl.hideAll());
        content.addView(hideAll);
        TextView hideAllHelp = new TextView(activity);
        hideAllHelp.setText(strings.getString(R.string.touch_hide_all_help));
        content.addView(hideAllHelp);

        // Qualcomm and Mali devices: open the GPU driver page (download / switch Turnip or PanVK).
        Button driverButton = null;
        if (GpuDriverStore.supported()) {
            driverButton = new Button(activity);
            GpuDriverStore.Installed driver = GpuDriverStore.selectedDriver(activity);
            driverButton.setText(strings.getString(R.string.touch_gpu_driver,
                driver != null ? driver.metadata.name : strings.getString(R.string.touch_gpu_driver_system)));
            driverButton.setAllCaps(false);
            content.addView(driverButton);
        }
        Button savesButton = new Button(activity);
        savesButton.setText(strings.getString(R.string.touch_saves_logs));
        savesButton.setAllCaps(false);
        content.addView(savesButton);
        Button folderButton = new Button(activity);
        folderButton.setText(strings.getString(R.string.touch_game_folder));
        folderButton.setAllCaps(false);
        content.addView(folderButton);

        ScrollView scroll = new ScrollView(activity);
        scroll.addView(content);
        AlertDialog settingsDialog = new AlertDialog.Builder(activity)
            .setTitle(strings.getString(R.string.touch_settings_title))
            .setView(scroll)
            .setPositiveButton(strings.getString(R.string.touch_apply), (dialog, which) ->
                applySettings(toggle.isChecked(), size.getProgress() + 60,
                              opacity.getProgress() + 25, ctrlAutoHide.isChecked(), hideAll.isChecked()))
            .setNeutralButton(strings.getString(R.string.touch_edit_layout), (dialog, which) -> {
                applySettings(toggle.isChecked(), size.getProgress() + 60,
                              opacity.getProgress() + 25, ctrlAutoHide.isChecked(), hideAll.isChecked());
                enterEditor();
            })
            .setNegativeButton(strings.getString(R.string.touch_cancel), null)
            .create();
        settingsDialog.setOnDismissListener(ignored -> {
            settingsToggle = null;
            controllerHint = null;
            settingsChoiceEdited = false;
            settingsDialogOpen = false;
            syncCtrlPin();
        });
        if (driverButton != null) {
            driverButton.setOnClickListener(v -> {
                settingsDialog.dismiss();
                activity.openGpuDriverPage();
            });
        }
        savesButton.setOnClickListener(v -> {
            settingsDialog.dismiss();
            activity.openSavesPage();
        });
        folderButton.setOnClickListener(v -> {
            settingsDialog.dismiss();
            activity.openGameFolderPage();
        });
        settingsDialogOpen = true;
        syncCtrlPin();
        settingsDialog.show();
    }

    private interface ProgressChanged { void update(int value); }

    private static SeekBar.OnSeekBarChangeListener seekListener(ProgressChanged listener) {
        return new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar seekBar, int progress,
                                                     boolean fromUser) {
                listener.update(progress);
            }
            @Override public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override public void onStopTrackingTouch(SeekBar seekBar) {}
        };
    }

    private void enterEditor() {
        clearTouches();
        draft = layout.copy();
        draft.clampAll(getWidth(), getHeight(), baseUnit * draft.size,
                       editorReservedBottom());
        editing = true;
        selected = -1;
        syncCtrlPin();
        publish();
    }

    private void leaveEditor(boolean save) {
        clearTouches();
        if (save) {
            layout = draft.copy();
            layout.clampAll(getWidth(), getHeight(), baseUnit * layout.size, 0f);
            saveLayout();
        }
        editing = false;
        draft = null;
        selected = -1;
        syncCtrlPin();
        publish();
    }

    private void resetDraft() {
        dragPointer = -1;
        float size = draft.size, opacity = draft.opacity;
        draft = TouchControlLayout.defaults(getWidth(), getHeight(), baseUnit * size);
        draft.size = size;
        draft.opacity = opacity;
        draft.clampAll(getWidth(), getHeight(), baseUnit * size, editorReservedBottom());
        selected = -1;
        invalidate();
    }

    private int toolbarAt(float x, float y) {
        if (Math.abs(y - getHeight() * .91f) > getHeight() * .052f) return -1;
        for (int i = 0; i < 4; ++i) {
            if (Math.abs(x - getWidth() * (.37f + i * .09f)) <= getWidth() * .043f)
                return i;
        }
        return -1;
    }

    private boolean onEditorTouch(MotionEvent event, int action, int index, int id) {
        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            float x = event.getX(index), y = event.getY(index);
            int toolbar = toolbarAt(x, y);
            if (toolbar >= 0) {
                if (toolbarPointer == -1) {
                    toolbarPointer = id;
                    toolbarPressed = toolbar;
                }
            } else if (dragPointer == -1) {
                // CTRL wins overlaps, as it does in play (#253).
                int element = onCtrl(x, y) ? TouchControlLayout.CTRL : elementAt(x, y, true);
                if (element == TouchControlLayout.CTRL) {
                    selected = element;
                    dragPointer = id;
                    dragOffsetX = settingX() - x;
                    dragOffsetY = settingY() - y;
                } else if (element >= 0) {
                    selected = element;
                    dragPointer = id;
                    dragOffsetX = px(element) - x;
                    dragOffsetY = py(element) - y;
                } else {
                    selected = -1;
                }
            }
            invalidate();
            return true;
        }
        if (action == MotionEvent.ACTION_MOVE) {
            if (dragPointer != -1) {
                for (int i = 0; i < event.getPointerCount(); ++i) {
                    if (event.getPointerId(i) == dragPointer) {
                        if (selected == TouchControlLayout.CTRL)
                            draft.moveCtrl(event.getX(i) + dragOffsetX,
                                           event.getY(i) + dragOffsetY, getWidth(),
                                           getHeight(), unit(), editorReservedBottom());
                        else
                            draft.move(selected, event.getX(i) + dragOffsetX,
                                       event.getY(i) + dragOffsetY, getWidth(), getHeight(),
                                       unit(), editorReservedBottom());
                        invalidate();
                        break;
                    }
                }
            }
            return true;
        }
        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP) {
            if (id == dragPointer) {
                dragPointer = -1;
                // CTRL dropped onto controls: move them clear so CTRL stays hittable.
                if (selected == TouchControlLayout.CTRL) {
                    draft.clampAll(getWidth(), getHeight(), unit(), editorReservedBottom());
                    invalidate();
                }
            }
            if (id == toolbarPointer) {
                int pressed = toolbarPressed;
                toolbarPointer = toolbarPressed = -1;
                if (pressed == toolbarAt(event.getX(index), event.getY(index))) {
                    if (pressed == 0) leaveEditor(true);
                    else if (pressed == 1) leaveEditor(false);
                    else if (pressed == 2) resetDraft();
                    else if (pressed == 3 && selected >= 0
                             && selected != TouchControlLayout.CTRL) {
                        dragPointer = -1;
                        draft.visible[selected] = !draft.visible[selected];
                        invalidate();
                    }
                }
            }
            return true;
        }
        if (action == MotionEvent.ACTION_CANCEL) {
            dragPointer = toolbarPointer = toolbarPressed = -1;
            return true;
        }
        return true;
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        // A mouse click belongs to SDL's pointer path, not to the virtual pad.
        if (!event.isFromSource(InputDevice.SOURCE_TOUCHSCREEN)
                && !event.isFromSource(InputDevice.SOURCE_STYLUS)) return false;
        if (event.getToolType(event.getActionIndex()) == MotionEvent.TOOL_TYPE_MOUSE) return false;
        if (layout == null) return false;
        int action = event.getActionMasked();
        int index = event.getActionIndex();
        int id = event.getPointerId(index);
        if (editing) return onEditorTouch(event, action, index, id);
        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            float x = event.getX(index), y = event.getY(index);
            long now = SystemClock.uptimeMillis();
            layoutCtrl(now);
            boolean onTab = ctrl.retracted() && ctrl.hitTab(x, y);
            if (ctrl.visible(now) && settingsPointer == -1 && (onTab
                    || (!ctrl.retracted() && onCtrl(x, y)))) {
                // A touch on the retracted tab only brings CTRL back.
                settingsPointer = id;
                settingsPointerRestores = onTab;
                ctrl.activity(now);
                syncCtrlPin();
                return true;
            }
            if (!controlsVisible()) return action == MotionEvent.ACTION_POINTER_DOWN;
            int element = elementAt(x, y, false);
            if (element < 0) return action == MotionEvent.ACTION_POINTER_DOWN;
            pointers.put(id, new Pointer(element, x, y));
            publish();
            return true;
        }
        if (action == MotionEvent.ACTION_MOVE) {
            for (int i = 0; i < event.getPointerCount(); ++i) {
                Pointer pointer = pointers.get(event.getPointerId(i));
                if (pointer != null) { pointer.x = event.getX(i); pointer.y = event.getY(i); }
            }
            publish();
            return true;
        }
        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP) {
            pointers.remove(id);
            if (id == settingsPointer) {
                boolean restoring = settingsPointerRestores;
                settingsPointer = -1;
                settingsPointerRestores = false;
                ctrl.activity(SystemClock.uptimeMillis());
                syncCtrlPin();
                if (!restoring && onCtrl(event.getX(index), event.getY(index))) {
                    performClick();
                    showSettings();
                    return true;
                }
            }
            publish();
            return true;
        }
        if (action == MotionEvent.ACTION_CANCEL) {
            clearTouches();
            return true;
        }
        return super.onTouchEvent(event);
    }

    @Override
    public boolean performClick() {
        super.performClick();
        return true;
    }

    @Override
    protected void onDetachedFromWindow() {
        stopControllerWatch();
        cancelCtrlWake();
        clearTouches();
        super.onDetachedFromWindow();
    }

    private void color(int value, float opacity) {
        paint.setColor(Color.argb(Math.round(Color.alpha(value) * opacity),
                                  Color.red(value), Color.green(value), Color.blue(value)));
    }

    private void drawButton(Canvas canvas, float x, float y, float radius,
                            String label, boolean active, float opacity) {
        paint.setStyle(Paint.Style.FILL);
        color(active ? Color.argb(190, 70, 159, 214) : Color.argb(90, 0, 0, 0), opacity);
        canvas.drawCircle(x, y, radius, paint);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(Math.max(2f, unit() * .015f));
        color(Color.argb(210, 222, 239, 249), opacity);
        canvas.drawCircle(x, y, radius, paint);
        paint.setStyle(Paint.Style.FILL);
        paint.setTextAlign(Paint.Align.CENTER);
        paint.setTextSize(unit() * .26f);
        paint.setFakeBoldText(false);
        color(Color.WHITE, opacity);
        canvas.drawText(label, x, y - (paint.ascent() + paint.descent()) * .5f, paint);
    }

    private void drawPill(Canvas canvas, float x, float y, String label,
                          boolean active, float opacity) {
        float halfWidth = unit() * .42f, halfHeight = unit() * .22f;
        paint.setStyle(Paint.Style.FILL);
        color(active ? Color.argb(190, 70, 159, 214) : Color.argb(90, 0, 0, 0), opacity);
        canvas.drawRoundRect(x - halfWidth, y - halfHeight, x + halfWidth,
                             y + halfHeight, halfHeight, halfHeight, paint);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(Math.max(2f, unit() * .015f));
        color(Color.argb(210, 222, 239, 249), opacity);
        canvas.drawRoundRect(x - halfWidth, y - halfHeight, x + halfWidth,
                             y + halfHeight, halfHeight, halfHeight, paint);
        paint.setStyle(Paint.Style.FILL);
        paint.setTextAlign(Paint.Align.CENTER);
        paint.setTextSize(unit() * .20f);
        paint.setFakeBoldText(true);
        color(Color.WHITE, opacity);
        canvas.drawText(label, x, y - (paint.ascent() + paint.descent()) * .5f, paint);
    }

    private void drawStick(Canvas canvas, float x, float y, int axisX, int axisY,
                           float opacity) {
        float unit = unit();
        paint.setStyle(Paint.Style.FILL);
        color(Color.argb(72, 0, 0, 0), opacity);
        canvas.drawCircle(x, y, unit, paint);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(Math.max(2f, unit * .015f));
        color(Color.argb(210, 222, 239, 249), opacity);
        canvas.drawCircle(x, y, unit, paint);
        color(Color.argb(92, 222, 239, 249), opacity);
        canvas.drawCircle(x, y, unit * .68f, paint);
        paint.setStyle(Paint.Style.FILL);
        color(axisX != 0 || axisY != 0
            ? Color.argb(220, 70, 159, 214) : Color.argb(180, 175, 185, 191), opacity);
        canvas.drawCircle(x + axisX / 32767f * unit * .55f,
                          y - axisY / 32767f * unit * .55f, unit * .40f, paint);
    }

    private void drawElement(Canvas canvas, int id) {
        TouchControlLayout controls = activeLayout();
        if (!editing && !controls.visible[id]) return;
        float opacity = editing ? (controls.visible[id] ? Math.max(.70f,
            controls.opacity) : .40f) : controls.opacity;
        float x = px(id), y = py(id), unit = unit();
        switch (id) {
            case TouchControlLayout.LEFT_STICK:
                drawStick(canvas, x, y, shownLx, shownLy, opacity); break;
            case TouchControlLayout.RIGHT_STICK:
                drawStick(canvas, x, y, shownRx, shownRy, opacity); break;
            case TouchControlLayout.DPAD:
                drawButton(canvas, x, y - unit * .855f, unit * .45f, "↑",
                           (shownButtons & UP) != 0, opacity);
                drawButton(canvas, x, y + unit * .855f, unit * .45f, "↓",
                           (shownButtons & DOWN) != 0, opacity);
                drawButton(canvas, x - unit * .855f, y, unit * .45f, "←",
                           (shownButtons & LEFT) != 0, opacity);
                drawButton(canvas, x + unit * .855f, y, unit * .45f, "→",
                           (shownButtons & RIGHT) != 0, opacity);
                break;
            case TouchControlLayout.BACK:
            case TouchControlLayout.START:
                drawPill(canvas, x, y, NAMES[id], (shownButtons & buttonMask(id)) != 0,
                         opacity);
                break;
            default:
                boolean active = id == TouchControlLayout.LT ? shownLt != 0
                    : id == TouchControlLayout.RT ? shownRt != 0
                    : (shownButtons & buttonMask(id)) != 0;
                drawButton(canvas, x, y, unit * .45f, NAMES[id], active, opacity);
                break;
        }
        if (editing && id == selected) {
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(Math.max(3f, unit * .025f));
            color(Color.argb(230, 255, 195, 67), 1f);
            canvas.drawCircle(x, y, TouchControlLayout.hitRadius(id) * unit * 1.08f, paint);
        }
    }

    private void drawEditorToolbar(Canvas canvas) {
        boolean ctrlSelected = selected == TouchControlLayout.CTRL;
        String description = selected < 0 ? "Drag a control · tap to select"
            : ctrlSelected ? "CTRL · always shown"
            : NAMES[selected] + (draft.visible[selected] ? " · visible" : " · hidden");
        paint.setStyle(Paint.Style.FILL);
        paint.setTextAlign(Paint.Align.CENTER);
        paint.setTextSize(Math.max(18f, unit() * .16f));
        paint.setFakeBoldText(true);
        color(Color.argb(240, 255, 255, 255), 1f);
        canvas.drawText(description, getWidth() * .5f, getHeight() * .82f, paint);
        for (int i = 0; i < 4; ++i) {
            float x = getWidth() * (.37f + i * .09f);
            float y = getHeight() * .91f;
            float halfWidth = getWidth() * .043f;
            float halfHeight = getHeight() * .045f;
            paint.setStyle(Paint.Style.FILL);
            color(Color.argb(i == toolbarPressed ? 220 : 185, 22, 33, 43), 1f);
            canvas.drawRoundRect(x - halfWidth, y - halfHeight, x + halfWidth,
                                 y + halfHeight, halfHeight * .45f,
                                 halfHeight * .45f, paint);
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(Math.max(2f, unit() * .012f));
            color(Color.argb(220, 222, 239, 249), 1f);
            canvas.drawRoundRect(x - halfWidth, y - halfHeight, x + halfWidth,
                                 y + halfHeight, halfHeight * .45f,
                                 halfHeight * .45f, paint);
            paint.setStyle(Paint.Style.FILL);
            paint.setTextAlign(Paint.Align.CENTER);
            paint.setTextSize(Math.max(11f * getResources().getDisplayMetrics().density,
                Math.min(unit() * .18f, getWidth() * .019f)));
            paint.setFakeBoldText(true);
            color(Color.WHITE, i == 3 && (selected < 0 || ctrlSelected) ? .45f : 1f);
            String label = i == 0 ? "SAVE" : i == 1 ? "CANCEL" : i == 2 ? "RESET"
                : selected >= 0 && !ctrlSelected && !draft.visible[selected] ? "SHOW" : "HIDE";
            canvas.drawText(label, x, y - (paint.ascent() + paint.descent()) * .5f, paint);
        }
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if (baseUnit <= 0f || activeLayout() == null) return;
        if (controlsVisible() || editing) {
            for (int id = 0; id < TouchControlLayout.COUNT; ++id) drawElement(canvas, id);
        }
        if (editing) {
            drawEditorCtrl(canvas);
            drawEditorToolbar(canvas);
        } else {
            drawCtrl(canvas);
        }
    }

    private void drawEditorCtrl(Canvas canvas) {
        float x = settingX(), y = settingY();
        drawButton(canvas, x, y, settingRadius(), "CTRL", false,
                   Math.max(.70f, draft.opacity));
        if (selected == TouchControlLayout.CTRL) {
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(Math.max(3f, unit() * .025f));
            color(Color.argb(230, 255, 195, 67), 1f);
            canvas.drawCircle(x, y, TouchControlLayout.CTRL_HIT_RADIUS * unit() * 1.08f, paint);
        }
    }

    private void drawCtrl(Canvas canvas) {
        long now = SystemClock.uptimeMillis();
        if (!ctrl.visible(now)) return;
        float progress = layoutCtrl(now);
        float radius = settingRadius();
        // Follows the layout's opacity slider (#253), fainter as a retracted tab.
        float opacity = layout.opacity * ctrl.opacity;
        boolean active = settingsPointer != -1 && !settingsPointerRestores;
        // The circle slides past the edge; the screen (or, at the top, the
        // status-bar strip) clips what is beyond it.
        canvas.save();
        canvas.clipRect(0f, ctrl.clipTop, getWidth(), getHeight());
        paint.setStyle(Paint.Style.FILL);
        color(active ? Color.argb(190, 70, 159, 214) : Color.argb(90, 0, 0, 0), opacity);
        canvas.drawCircle(ctrl.drawX, ctrl.drawY, radius, paint);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(Math.max(2f, unit() * .015f));
        color(Color.argb(210, 222, 239, 249), opacity);
        canvas.drawCircle(ctrl.drawX, ctrl.drawY, radius, paint);
        paint.setStyle(Paint.Style.FILL);
        paint.setTextAlign(Paint.Align.CENTER);
        paint.setTextSize(unit() * .26f * ctrl.labelScale);
        paint.setFakeBoldText(progress > .5f);
        color(Color.WHITE, Math.min(1f, opacity * 1.5f));
        float baseline = ctrl.labelY - (paint.ascent() + paint.descent()) * .5f;
        if (ctrl.labelRotation != 0f) {
            canvas.save();
            canvas.rotate(ctrl.labelRotation, ctrl.labelX, ctrl.labelY);
            canvas.drawText("CTRL", ctrl.labelX, baseline, paint);
            canvas.restore();
        } else {
            canvas.drawText("CTRL", ctrl.labelX, baseline, paint);
        }
        canvas.restore();
        paint.setFakeBoldText(false);
        scheduleCtrlWake(now);
    }
}
