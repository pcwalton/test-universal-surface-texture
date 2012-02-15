package org.mozilla.testuniversalsurfacetexture;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.opengl.GLSurfaceView;
import android.opengl.GLU;
import android.opengl.GLUtils;
import android.os.Bundle;
import android.os.Process;
import android.os.SystemClock;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Display;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;
import android.widget.RelativeLayout;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class TestUniversalSurfaceTexture extends Activity implements GLSurfaceView.Renderer,
                                                                     SurfaceHolder.Callback {
    private GLSurfaceView mGLSurfaceView;
    private SurfaceView mSurfaceView;
    private Square mSquare;
    private int mFrameCount;

    private Surface mSurface;
    private boolean mSurfaceDestroyed;

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mSurfaceDestroyed = false;

        mSquare = new Square();

        final DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(metrics);

        // The SurfaceView must start out on screen to avoid crashes on PowerVR.
        RelativeLayout layout = new RelativeLayout(this);
        RelativeLayout.LayoutParams params = new RelativeLayout.LayoutParams(512, 512);
        params.leftMargin = metrics.widthPixels - 1;
        params.topMargin = 0;

        mSurfaceView = new SurfaceView(this) {
            @Override
            protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
                // If this is not done, the surface gets clipped to the visible rect.
                setMeasuredDimension(512, 512);
            }
        };

        mSurfaceView.getHolder().addCallback(this);
        layout.addView(mSurfaceView, params);

        mGLSurfaceView = new GLSurfaceView(this);
        mGLSurfaceView.setRenderer(this);

        params = new RelativeLayout.LayoutParams(metrics.widthPixels - 1, metrics.heightPixels);
        params.leftMargin = 0;
        params.topMargin = 0;
        layout.addView(mGLSurfaceView, params);

        setContentView(layout);

        /*try {
            Method method = SurfaceView.class.getMethod("setWindowType", Integer.TYPE);
            method.invoke(mSurfaceView, WindowManager.LayoutParams.TYPE_WALLPAPER);
        } catch (IllegalAccessException e) {
            throw new RuntimeException(e);
        } catch (InvocationTargetException e) {
            throw new RuntimeException(e);
        } catch (NoSuchMethodException e) {
            throw new RuntimeException(e);
        }*/

        /*try {
            Class surfaceSessionClass = Class.forName("android.view.SurfaceSession");
            Object surfaceSession = surfaceSessionClass.newInstance();
            Constructor<?>[] constructors = Surface.class.getConstructors();
            Constructor<?> surfaceConstructor = null;
            for (Constructor<?> constructor : constructors) {
                if (constructor.getParameterTypes().length == 7) {
                    surfaceConstructor = constructor;
                    break;
                }
            }

            Display display = getWindowManager().getDefaultDisplay();
            Object mySurface = surfaceConstructor.newInstance(surfaceSession, Process.myPid(),
                                                              display.getDisplayId(), 512, 512,
                                                              PixelFormat.RGB_565, 0);
            Log.e("TUST", "### Surface is " + mySurface);
        } catch (ClassNotFoundException e) {
            throw new RuntimeException(e);
        } catch (InstantiationException e) {
            throw new RuntimeException(e);
        } catch (IllegalAccessException e) {
            throw new RuntimeException(e);
        } catch (InvocationTargetException e) {
            throw new RuntimeException(e);
        }*/
    }

    // SurfaceView interface

    public void surfaceChanged(final SurfaceHolder holder, int format, int width, int height) {
        Log.e("TUST", "### surfaceChanged " + format + " " + width + " " + height + " " +
              ((holder.getSurface() == null) ? "null!" : holder.getSurface()));

        synchronized (this) {
            mSurface = holder.getSurface();
            try {
                Canvas c = mSurface.lockCanvas(new Rect(0, 0, 512, 512));
                c.drawColor(Color.BLUE);
                mSurface.unlockCanvasAndPost(c);
            } catch (Surface.OutOfResourcesException e) {
                throw new RuntimeException(e);
            }

            notifyAll();
        }
    }

    private synchronized void paint() {
        if (mSurface == null) {
            return;
        }

        try {
            Canvas c = mSurface.lockCanvas(new Rect(0, 0, 512, 512));
            c.drawColor(Color.rgb(mFrameCount % 256, mFrameCount % 256, mFrameCount % 256));
            mSurface.unlockCanvasAndPost(c);
        } catch (Surface.OutOfResourcesException e) {
            throw new RuntimeException(e);
        }
    }

    public void surfaceCreated(SurfaceHolder holder) {
        Log.e("TUST", "### surfaceCreated");
        Canvas c = holder.lockCanvas();
        c.drawColor(Color.BLUE);
        holder.unlockCanvasAndPost(c);

        mSurfaceView.post(new Runnable() {
            public void run() {
                RelativeLayout.LayoutParams params = new RelativeLayout.LayoutParams(512, 512);
                params.leftMargin = 1000;
                params.topMargin = 0;
                mSurfaceView.setLayoutParams(params);
            }
        });
    }

    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.e("TUST", "### surfaceDestroyed");
        synchronized (this) {
            mSurface = null;
            mSurfaceDestroyed = true;
            this.notifyAll();
        }
    }

    // GLSurfaceView interface

	/**
	 * The Surface is created/init()
	 */
	public void onSurfaceCreated(GL10 gl, EGLConfig config) {		
		gl.glEnable(GL10.GL_TEXTURE_2D);			//Enable Texture Mapping ( NEW )
		gl.glShadeModel(GL10.GL_SMOOTH); 			//Enable Smooth Shading
		gl.glClearColor(0.0f, 0.0f, 0.0f, 0.5f); 	//Black Background
		gl.glClearDepthf(1.0f); 					//Depth Buffer Setup
		gl.glEnable(GL10.GL_DEPTH_TEST); 			//Enables Depth Testing
		gl.glDepthFunc(GL10.GL_LEQUAL); 			//The Type Of Depth Testing To Do
		
		//Really Nice Perspective Calculations
		gl.glHint(GL10.GL_PERSPECTIVE_CORRECTION_HINT, GL10.GL_NICEST); 

		//Load the texture for the square once during Surface creation
		mSquare.loadGLTexture(gl, this);
	}

	/**
	 * Here we do our drawing
	 */
	public void onDrawFrame(GL10 gl) {
		//Clear Screen And Depth Buffer
		gl.glClear(GL10.GL_COLOR_BUFFER_BIT | GL10.GL_DEPTH_BUFFER_BIT);	
		gl.glLoadIdentity();					//Reset The Current Modelview Matrix
		
		/*
		 * Minor changes to the original tutorial
		 * 
		 * Instead of drawing our objects here,
		 * we fire their own drawing methods on
		 * the current instance
		 */
		gl.glTranslatef(0.0f, -1.2f, -6.0f);	//Move down 1.2 Unit And Into The Screen 6.0
		mSquare.draw(gl);						//Draw the square
	}

	/**
	 * If the surface changes, reset the view
	 */
	public void onSurfaceChanged(GL10 gl, int width, int height) {
		if(height == 0) { 						//Prevent A Divide By Zero By
			height = 1; 						//Making Height Equal One
		}

		gl.glViewport(0, 0, width, height); 	//Reset The Current Viewport
		gl.glMatrixMode(GL10.GL_PROJECTION); 	//Select The Projection Matrix
		gl.glLoadIdentity(); 					//Reset The Projection Matrix

		//Calculate The Aspect Ratio Of The Window
		GLU.gluPerspective(gl, 45.0f, (float)width / (float)height, 0.1f, 100.0f);

		gl.glMatrixMode(GL10.GL_MODELVIEW); 	//Select The Modelview Matrix
		gl.glLoadIdentity(); 					//Reset The Modelview Matrix
	}

    private class Square {
            
        /** The buffer holding the vertices */
        private FloatBuffer vertexBuffer;
        /** The buffer holding the texture coordinates */
        private FloatBuffer textureBuffer;
        /** Our texture pointer */
        private int[] textures = new int[1];
        
        /** The initial vertex definition */
        private float vertices[] = { 
                                    -1.0f, -1.0f, 0.0f, //Bottom Left
                                    1.0f, -1.0f, 0.0f, 	//Bottom Right
                                    -1.0f, 1.0f, 0.0f, 	//Top Left
                                    1.0f, 1.0f, 0.0f 	//Top Right
                                                    };

        private float texture[] = {    		
                            //Mapping coordinates for the vertices
                            0.0f, 0.0f,
                            0.0f, 1.0f,
                            1.0f, 0.0f,
                            1.0f, 1.0f
        };
        
        /**
         * The Square constructor.
         * 
         * Initiate the buffers.
         */
        public Square() {
            //
            ByteBuffer byteBuf = ByteBuffer.allocateDirect(vertices.length * 4);
            byteBuf.order(ByteOrder.nativeOrder());
            vertexBuffer = byteBuf.asFloatBuffer();
            vertexBuffer.put(vertices);
            vertexBuffer.position(0);

            byteBuf = ByteBuffer.allocateDirect(texture.length * 4);
            byteBuf.order(ByteOrder.nativeOrder());
            textureBuffer = byteBuf.asFloatBuffer();
            textureBuffer.put(texture);
            textureBuffer.position(0);
        }

        /**
         * The object own drawing function.
         * Called from the renderer to redraw this instance
         * with possible changes in values.
         * 
         * @param gl - The GL context
         */
        public void draw(GL10 gl) {
            mFrameCount++;
            if (mFrameCount % 30 == 0) {
                //paint();
            }

            gl.glBindTexture(GL10.GL_TEXTURE_2D, textures[0]);
            attachTexture(mSurface, mSurfaceDestroyed);

            //Set the face rotation
            gl.glFrontFace(GL10.GL_CW);
            
            //Point to our vertex buffer
            gl.glVertexPointer(3, GL10.GL_FLOAT, 0, vertexBuffer);
            gl.glTexCoordPointer(2, GL10.GL_FLOAT, 0, textureBuffer);
            
            //Enable vertex buffer
            gl.glEnableClientState(GL10.GL_VERTEX_ARRAY);
            gl.glEnableClientState(GL10.GL_TEXTURE_COORD_ARRAY);
            
            //Draw the vertices as triangle strip
            gl.glDrawArrays(GL10.GL_TRIANGLE_STRIP, 0, vertices.length / 3);
            
            //Disable the client state before leaving
            gl.glDisableClientState(GL10.GL_VERTEX_ARRAY);
            gl.glDisableClientState(GL10.GL_TEXTURE_COORD_ARRAY);
        }

        /**
         * Load the textures
         * 
         * @param gl - The GL Context
         * @param context - The Activity context
         */
        public void loadGLTexture(GL10 gl, Context context) {
            Log.e("TUST", "### Loading GL texture");

            //Generate one texture pointer...
            gl.glGenTextures(1, textures, 0);
            //...and bind it to our array
            gl.glBindTexture(GL10.GL_TEXTURE_2D, textures[0]);
            
            //Create Nearest Filtered Texture
            gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_MIN_FILTER, GL10.GL_NEAREST);
            gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_MAG_FILTER, GL10.GL_LINEAR);

            //Different possible texture parameters, e.g. GL10.GL_CLAMP_TO_EDGE
            gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_WRAP_S, GL10.GL_REPEAT);
            gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_WRAP_T, GL10.GL_REPEAT);
        }
    }

    static {
        System.loadLibrary("ndk1");
    }

    private synchronized void attachTextureToSurface() {
        synchronized (TestUniversalSurfaceTexture.this) {
            try {
                while (mSurface == null) {
                    this.wait();
                }
                attachTexture(mSurface, mSurfaceDestroyed);
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }
        }
    }

    public static native void attachTexture(Surface surface, boolean surfaceDestroyed);
}
