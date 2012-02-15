/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   James Willcox <jwillcox@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#define EGL_EGLEXT_PROTOTYPES

#include <jni.h>
#include <cassert>
#include <cstdint>
#include <dlfcn.h>
#include <android/log.h>
#include <sys/time.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <GLES2/gl2.h>
#include <map>

/*
 * Android stuff
 */

enum {
    /* buffer is never read in software */
    GRALLOC_USAGE_SW_READ_NEVER   = 0x00000000,
    /* buffer is rarely read in software */
    GRALLOC_USAGE_SW_READ_RARELY  = 0x00000002,
    /* buffer is often read in software */
    GRALLOC_USAGE_SW_READ_OFTEN   = 0x00000003,
    /* mask for the software read values */
    GRALLOC_USAGE_SW_READ_MASK    = 0x0000000F,

    /* buffer is never written in software */
    GRALLOC_USAGE_SW_WRITE_NEVER  = 0x00000000,
    /* buffer is never written in software */
    GRALLOC_USAGE_SW_WRITE_RARELY = 0x00000020,
    /* buffer is never written in software */
    GRALLOC_USAGE_SW_WRITE_OFTEN  = 0x00000030,
    /* mask for the software write values */
    GRALLOC_USAGE_SW_WRITE_MASK   = 0x000000F0,

    /* buffer will be used as an OpenGL ES texture */
    GRALLOC_USAGE_HW_TEXTURE      = 0x00000100,
    /* buffer will be used as an OpenGL ES render target */
    GRALLOC_USAGE_HW_RENDER       = 0x00000200,
    /* buffer will be used by the 2D hardware blitter */
    GRALLOC_USAGE_HW_2D           = 0x00000400,
    /* buffer will be used with the framebuffer device */
    GRALLOC_USAGE_HW_FB           = 0x00001000,
    /* mask for the software usage bit-mask */
    GRALLOC_USAGE_HW_MASK         = 0x00001F00,
};

struct hw_module_methods_t;
typedef void* buffer_handle_t;

typedef struct hw_module_t {
    /** tag must be initialized to HARDWARE_MODULE_TAG */
    uint32_t tag;

    /** major version number for the module */
    uint16_t version_major;

    /** minor version number of the module */
    uint16_t version_minor;

    /** Identifier of module */
    const char *id;

    /** Name of this module */
    const char *name;

    /** Author/owner/implementor of the module */
    const char *author;

    /** Modules methods */
    struct hw_module_methods_t* methods;

    /** module's dso */
    void* dso;

    /** padding to 128 bytes, reserved for future use */
    uint32_t reserved[32-7];

} hw_module_t;

typedef struct gralloc_module_t {
    struct hw_module_t common;
    
    /*
     * (*registerBuffer)() must be called before a buffer_handle_t that has not
     * been created with (*alloc_device_t::alloc)() can be used.
     * 
     * This is intended to be used with buffer_handle_t's that have been
     * received in this process through IPC.
     * 
     * This function checks that the handle is indeed a valid one and prepares
     * it for use with (*lock)() and (*unlock)().
     * 
     * It is not necessary to call (*registerBuffer)() on a handle created 
     * with (*alloc_device_t::alloc)().
     * 
     * returns an error if this buffer_handle_t is not valid.
     */
    int (*registerBuffer)(struct gralloc_module_t const* module,
            buffer_handle_t handle);

    /*
     * (*unregisterBuffer)() is called once this handle is no longer needed in
     * this process. After this call, it is an error to call (*lock)(),
     * (*unlock)(), or (*registerBuffer)().
     * 
     * This function doesn't close or free the handle itself; this is done
     * by other means, usually through libcutils's native_handle_close() and
     * native_handle_free(). 
     * 
     * It is an error to call (*unregisterBuffer)() on a buffer that wasn't
     * explicitly registered first.
     */
    int (*unregisterBuffer)(struct gralloc_module_t const* module,
            buffer_handle_t handle);
    
    /*
     * The (*lock)() method is called before a buffer is accessed for the 
     * specified usage. This call may block, for instance if the h/w needs
     * to finish rendering or if CPU caches need to be synchronized.
     * 
     * The caller promises to modify only pixels in the area specified 
     * by (l,t,w,h).
     * 
     * The content of the buffer outside of the specified area is NOT modified
     * by this call.
     *
     * If usage specifies GRALLOC_USAGE_SW_*, vaddr is filled with the address
     * of the buffer in virtual memory.
     *
     * THREADING CONSIDERATIONS:
     *
     * It is legal for several different threads to lock a buffer from 
     * read access, none of the threads are blocked.
     * 
     * However, locking a buffer simultaneously for write or read/write is
     * undefined, but:
     * - shall not result in termination of the process
     * - shall not block the caller
     * It is acceptable to return an error or to leave the buffer's content
     * into an indeterminate state.
     *
     * If the buffer was created with a usage mask incompatible with the
     * requested usage flags here, -EINVAL is returned. 
     * 
     */
    
    int (*lock)(struct gralloc_module_t const* module,
            buffer_handle_t handle, int usage,
            int l, int t, int w, int h,
            void** vaddr);

    
    /*
     * The (*unlock)() method must be called after all changes to the buffer
     * are completed.
     */
    
    int (*unlock)(struct gralloc_module_t const* module,
            buffer_handle_t handle);


    /* reserved for future use */
    int (*perform)(struct gralloc_module_t const* module,
            int operation, ... );

    /* reserved for future use */
    void* reserved_proc[7];
} gralloc_module_t;

typedef void* buffer_handle_t;

typedef struct android_native_base_t
{
    /* a magic value defined by the actual EGL native type */
    int magic;
    
    /* the sizeof() of the actual EGL native type */
    int version;

    void* reserved[4];

    /* reference-counting interface */
    void (*incRef)(struct android_native_base_t* base);
    void (*decRef)(struct android_native_base_t* base);
} android_native_base_t;

struct ANativeWindow 
{
    struct android_native_base_t common;

    /* flags describing some attributes of this surface or its updater */
    const uint32_t flags;
    
    /* min swap interval supported by this updated */
    const int   minSwapInterval;

    /* max swap interval supported by this updated */
    const int   maxSwapInterval;

    /* horizontal and vertical resolution in DPI */
    const float xdpi;
    const float ydpi;

    /* Some storage reserved for the OEM's driver. */
    intptr_t    oem[4];
        

    /*
     * Set the swap interval for this surface.
     * 
     * Returns 0 on success or -errno on error.
     */
    int     (*setSwapInterval)(struct ANativeWindow* window,
                int interval);
    
    /*
     * hook called by EGL to acquire a buffer. After this call, the buffer
     * is not locked, so its content cannot be modified.
     * this call may block if no buffers are available.
     * 
     * Returns 0 on success or -errno on error.
     */
    int     (*dequeueBuffer)(struct ANativeWindow* window,
                struct android_native_buffer_t** buffer);

    /*
     * hook called by EGL to lock a buffer. This MUST be called before modifying
     * the content of a buffer. The buffer must have been acquired with 
     * dequeueBuffer first.
     * 
     * Returns 0 on success or -errno on error.
     */
    int     (*lockBuffer)(struct ANativeWindow* window,
                struct android_native_buffer_t* buffer);
   /*
    * hook called by EGL when modifications to the render buffer are done. 
    * This unlocks and post the buffer.
    * 
    * Buffers MUST be queued in the same order than they were dequeued.
    * 
    * Returns 0 on success or -errno on error.
    */
    int     (*queueBuffer)(struct ANativeWindow* window,
                struct android_native_buffer_t* buffer);

    /*
     * hook used to retrieve information about the native window.
     * 
     * Returns 0 on success or -errno on error.
     */
    int     (*query)(struct ANativeWindow* window,
                int what, int* value);
    
    /*
     * hook used to perform various operations on the surface.
     * (*perform)() is a generic mechanism to add functionality to
     * ANativeWindow while keeping backward binary compatibility.
     * 
     * This hook should not be called directly, instead use the helper functions
     * defined below.
     * 
     *  (*perform)() returns -ENOENT if the 'what' parameter is not supported
     *  by the surface's implementation.
     *
     * The valid operations are:
     *     NATIVE_WINDOW_SET_USAGE
     *     NATIVE_WINDOW_CONNECT
     *     NATIVE_WINDOW_DISCONNECT
     *     NATIVE_WINDOW_SET_CROP
     *     NATIVE_WINDOW_SET_BUFFER_COUNT
     *     NATIVE_WINDOW_SET_BUFFERS_GEOMETRY
     *     NATIVE_WINDOW_SET_BUFFERS_TRANSFORM
     *  
     */
    
    int     (*perform)(struct ANativeWindow* window,
                int operation, ... );
    
    /*
     * hook used to cancel a buffer that has been dequeued.
     * No synchronization is performed between dequeue() and cancel(), so
     * either external synchronization is needed, or these functions must be
     * called from the same thread.
     */
    int     (*cancelBuffer)(struct ANativeWindow* window,
                struct android_native_buffer_t* buffer);


    void* reserved_proc[2];
};

struct android_native_buffer_t {
    android_native_base_t common;

    int width;
    int height;
    int stride;
    int format;
    int usage;
    
    void* reserved[2];

    buffer_handle_t handle;

    void* reserved_proc[8];
};

enum {
    NATIVE_WINDOW_SET_USAGE  = 0,
    NATIVE_WINDOW_CONNECT,
    NATIVE_WINDOW_DISCONNECT,
    NATIVE_WINDOW_SET_CROP,
    NATIVE_WINDOW_SET_BUFFER_COUNT,
    NATIVE_WINDOW_SET_BUFFERS_GEOMETRY,
    NATIVE_WINDOW_SET_BUFFERS_TRANSFORM,
};

jclass sSurfaceClass = 0;
jfieldID sNativeSurfaceField = 0;
jfieldID sSurfaceControlField = 0;

struct EGLImages {
    EGLImageKHR a;
    EGLImageKHR b;
};

struct Rect {
    int left;
    int top;
    int right;
    int bottom;
};

static EGLImageKHR sImage = 0;
static android_native_buffer_t* sBuffer = 0;
static gralloc_module_t *sModule;

int (*sw_gralloc_handle_t_lock)(void* handle, int usage, int x, int y, int width, int height,
                                 void** addr) = 0;
int (*sw_gralloc_handle_t_unlock)(void* handle) = 0;

void (*hw_get_module)(const char* id, hw_module_t** module);


extern "C" void
Java_org_mozilla_testuniversalsurfacetexture_TestUniversalSurfaceTexture_attachTexture(JNIEnv*
    aJEnv, jclass klass, jobject aSurface, int aDestroyed)
{
    __android_log_print(ANDROID_LOG_ERROR, "TUST", "### point a");

    if (!sSurfaceClass) {
        sSurfaceClass = reinterpret_cast<jclass>
            (aJEnv->NewGlobalRef(aJEnv->FindClass("android/view/Surface")));
        sNativeSurfaceField = aJEnv->GetFieldID(sSurfaceClass, "mNativeSurface", "I");
        sSurfaceControlField = aJEnv->GetFieldID(sSurfaceClass, "mSurfaceControl", "I");

        void* lib = dlopen("libui.so", RTLD_LAZY);
        sw_gralloc_handle_t_lock = (typeof(sw_gralloc_handle_t_lock))
            dlsym(lib, "_ZN7android19sw_gralloc_handle_t4lockEPS0_iiiiiPPv");
        sw_gralloc_handle_t_unlock = (typeof(sw_gralloc_handle_t_unlock))
            dlsym(lib, "_ZN7android19sw_gralloc_handle_t6unlockEPS0_");
        __android_log_print(ANDROID_LOG_ERROR, "TUST", "### Lock=%p, unlock=%p",
                            sw_gralloc_handle_t_lock, sw_gralloc_handle_t_unlock);

        lib = dlopen("libhardware.so", RTLD_LAZY);
        hw_get_module = (typeof(hw_get_module))dlsym(lib, "hw_get_module");

        hw_module_t* pModule;
        hw_get_module("gralloc", &pModule);
        sModule = reinterpret_cast<gralloc_module_t*>(pModule);
        __android_log_print(ANDROID_LOG_ERROR, "TUST", "### Gralloc module=%p", pModule);
    }

    if (!sImage) {
        ANativeWindow* nativeWindow = reinterpret_cast<ANativeWindow*>
            (aJEnv->GetIntField(aSurface, sNativeSurfaceField) + 8);

        /*Rect rect;
        rect.left = rect.top = rect.right = rect.bottom = 0;
        nativeWindow->perform(nativeWindow, NATIVE_WINDOW_SET_CROP, &rect);*/

        // NB: nativeWindow->common.magic is '_wnd' as a FourCC.
        // My version is 104.

        __android_log_print(ANDROID_LOG_ERROR, "TUST",
                            "### Native window ptr %p, magic %08x, version %d, reserved %p, flags %d, dpi %g\n",
                            nativeWindow, (unsigned)nativeWindow->common.magic,
                            nativeWindow->common.version, nativeWindow->common.reserved[0],
                            (int)nativeWindow->flags, (double)nativeWindow->xdpi);

        nativeWindow->dequeueBuffer(nativeWindow, &sBuffer);
        nativeWindow->lockBuffer(nativeWindow, sBuffer);

        // Must increment the refcount on the native window to avoid crashes on Mali (Galaxy S2).
        nativeWindow->common.incRef(&nativeWindow->common);

        const EGLint eglImgAttrs[] = { EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE, EGL_NONE };

        sBuffer->common.incRef(&sBuffer->common);

        sImage = eglCreateImageKHR(eglGetDisplay(EGL_DEFAULT_DISPLAY), EGL_NO_CONTEXT,
                                     EGL_NATIVE_BUFFER_ANDROID,
                                     reinterpret_cast<EGLClientBuffer>(sBuffer), eglImgAttrs);

        //nativeWindow->queueBuffer(nativeWindow, sBuffer);
    }

    uint8_t *bits = 0;
    int err = sModule->lock(sModule, sBuffer->handle, GRALLOC_USAGE_SW_READ_OFTEN |
                            GRALLOC_USAGE_SW_WRITE_OFTEN, 0, 0, 512, 512, (void**)&bits);
    /*__android_log_print(ANDROID_LOG_ERROR, "TUST",
                "### Buffer width=%d height=%d stride=%d format=%d usage=%d Bits are: %p, err=%d",
                sBuffer->width, sBuffer->height, sBuffer->stride, sBuffer->format, sBuffer->usage,
                bits, err);*/

    struct timeval tv;
    gettimeofday(&tv, NULL);

    static int x = 0;

    for (int i = 0; i < 512*512*2; i += 2) {
        bits[i] = bits[i+1] = (tv.tv_usec / 100000) % 256;
    }
    sModule->unlock(sModule, sBuffer->handle);

    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, sImage);

    /*__android_log_print(ANDROID_LOG_ERROR, "TUST", "### Success! GL error is: %d",
                        (int)glGetError());*/
}

