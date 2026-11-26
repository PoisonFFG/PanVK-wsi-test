package com.example.panvkwsi;

import android.app.Activity;
import android.os.Bundle;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.view.Gravity;
import android.graphics.Color;

public class MainActivity extends Activity implements SurfaceHolder.Callback {
    static { System.loadLibrary("panvk_wsi_test"); }

    private TextView status;
    private volatile boolean testStarted = false;

    private native String runTest(Surface surface);

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setBackgroundColor(Color.rgb(245, 245, 245));

        status = new TextView(this);
        status.setTextColor(Color.BLACK);
        status.setTextSize(17);
        status.setPadding(24, 20, 24, 20);
        status.setGravity(Gravity.CENTER_VERTICAL);
        status.setText("PanVK Android WSI test\n\nWaiting for Android Surface…");
        layout.addView(status, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 0, 1));

        SurfaceView surfaceView = new SurfaceView(this);
        surfaceView.getHolder().addCallback(this);
        layout.addView(surfaceView, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 0, 3));

        setContentView(layout);
    }

    @Override public void surfaceCreated(SurfaceHolder holder) {}

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        if (testStarted || width <= 0 || height <= 0) return;
        testStarted = true;
        status.setText("Surface created: " + width + " x " + height +
                "\nRunning Vulkan instance/device/swapchain/acquire/present test…");
        final Surface surface = holder.getSurface();
        new Thread(() -> {
            String result;
            try {
                result = runTest(surface);
            } catch (Throwable t) {
                result = "Java-side failure: " + t;
            }
            final String finalResult = result;
            runOnUiThread(() -> status.setText(finalResult));
        }, "Vulkan-WSI-Test").start();
    }

    @Override public void surfaceDestroyed(SurfaceHolder holder) {}
}
