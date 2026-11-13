package io.github.freefrank.lostodyssey;

import android.app.Activity;
import android.app.AlertDialog;
import android.widget.ArrayAdapter;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.CheckBox;
import android.widget.Button;

final class TvSettingsDialog {
    private TvSettingsDialog() {}
    static void show(Activity activity, TvOutputController output, TvDisplayDiagnostics diagnostics) {
        LinearLayout content = new LinearLayout(activity);
        content.setOrientation(LinearLayout.VERTICAL);
        int padding = Math.round(20 * activity.getResources().getDisplayMetrics().density);
        content.setPadding(padding, padding, padding, padding);
        TextView status = new TextView(activity);
        status.setText(output.status() + "\n\n" + activity.getString(R.string.tv_explanation));
        content.addView(status);
        Spinner mode = choice(activity, content, R.string.tv_mode, R.array.tv_modes, output.mode());
        Spinner resolution = choice(activity, content, R.string.tv_resolution, R.array.tv_resolutions,
            output.height() == 720 ? 0 : 1);
        CheckBox touch = new CheckBox(activity);
        touch.setText(R.string.tv_hide_touch);
        touch.setChecked(output.preferences.getBoolean("hide_touch", true));
        content.addView(touch);
        CheckBox dim = new CheckBox(activity);
        dim.setText(R.string.tv_dim_phone);
        dim.setChecked(output.preferences.getBoolean("dim_phone", false));
        content.addView(dim);
        TextView audio = new TextView(activity);
        audio.setText(R.string.tv_audio_explanation);
        content.addView(audio);
        Button tone = new Button(activity);
        tone.setText(R.string.tv_test_audio);
        tone.setOnClickListener(v -> TvAudioProbe.play(activity, diagnostics));
        content.addView(tone);
        Button report = new Button(activity);
        report.setText(R.string.tv_diagnostics);
        report.setOnClickListener(v -> diagnostics.snapshot("manual"));
        content.addView(report);
        ScrollView scroll = new ScrollView(activity);
        scroll.addView(content);
        new AlertDialog.Builder(activity).setTitle(R.string.tv_mode).setView(scroll)
            .setPositiveButton(R.string.tv_apply, (dialog, which) -> {
                output.preferences.edit().putInt("mode", mode.getSelectedItemPosition())
                    .putInt("height", resolution.getSelectedItemPosition() == 0 ? 720 : 1080)
                    .putBoolean("hide_touch", touch.isChecked()).putBoolean("dim_phone", dim.isChecked()).apply();
                output.settingsChanged();
            }).setNegativeButton(android.R.string.cancel, null).show();
    }
    private static Spinner choice(Activity activity, LinearLayout content, int label, int values, int selected) {
        TextView title = new TextView(activity);
        title.setText(label);
        content.addView(title);
        Spinner spinner = new Spinner(activity);
        ArrayAdapter<CharSequence> adapter = ArrayAdapter.createFromResource(activity, values, android.R.layout.simple_spinner_item);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinner.setAdapter(adapter);
        spinner.setSelection(selected);
        content.addView(spinner);
        return spinner;
    }
}
