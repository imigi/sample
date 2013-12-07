package com.example.ndkbook.OpenMax;

import android.content.Context;
import android.graphics.PixelFormat;
import android.opengl.GLSurfaceView;
import android.view.SurfaceHolder;
import android.view.View;
import android.view.View.OnClickListener;

class OpenGL2View extends GLSurfaceView implements SurfaceHolder.Callback {
    private static String TAG = "GL2View";
    private MyRenderer renderer;

    public OpenGL2View(Context context) {
        super(context);
        // SurfaceHolderのフォーマットを透過するものへ変更
        this.getHolder().setFormat(PixelFormat.TRANSLUCENT);

        // Open GL ES 2.0向けContextFactoryを設定
        setEGLContextFactory(new MyContextFactory());

        // RGBA = (8, 8, 8, 8)
        // depth buffer size = 0、 stencil buffer size = 0のConfigを選ぶように設定
        setEGLConfigChooser(new MyEGLConfigChooser());

        // Rendererをセット
        renderer = new MyRenderer();
        setRenderer(renderer);

    }

    public MyRenderer getRenderer() {
        return renderer;
    }

    @Override
    public void onPause() {
        if (renderer != null)
            renderer.onPause();
        super.onPause();
    }

    @Override
    public void onResume() {
        super.onResume();
        if (renderer != null)
            renderer.onResume();
    }

    public void onDestroy() {
        renderer.onDestroy();
    }
}
