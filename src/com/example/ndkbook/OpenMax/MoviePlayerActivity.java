package com.example.ndkbook.OpenMax;

import android.app.Activity;
import android.opengl.GLSurfaceView.Renderer;
import android.os.Bundle;
import android.widget.FrameLayout;

public class MoviePlayerActivity extends Activity {
    private OpenGL2View openGL2View;

    @Override
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        openGL2View = new OpenGL2View(getApplication());
        FrameLayout frameLayout = new FrameLayout(this);
        frameLayout.addView(openGL2View);
        setContentView(frameLayout);

        MoviePlayer.engineSetup();
    }

    @Override
    protected void onDestroy() {
        openGL2View.onDestroy();
        MoviePlayer.engineShutdown();
        super.onDestroy();
    }

    @Override
    protected void onPause() {
        super.onPause();
        openGL2View.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        openGL2View.onResume();
    }

}
