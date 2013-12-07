package com.example.ndkbook.OpenMax;

public class MoviePlayerNative {

    static {
        System.loadLibrary("movieplayer");
    }

    public static native void init(int width, int height);

    public static native void step(int textureId, int textureWidth,
            int textureHeight,float[] stMatrix);
}
