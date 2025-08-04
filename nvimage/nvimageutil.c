/* GStreamer
 * Copyright (C) <2005> Luca Ognibene <luogni@tin.it>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "nvimageutil.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static gboolean nvimageutil_fbccontext_get(GstXContext *xcontext);
static gboolean nvimageutil_fbccontext_clear(GstXContext *xcontext);
static gboolean nvimageutil_xcontext_get (GstXContext *xcontext, GstElement * parent, const gchar * display_name);
static void nvimageutil_xcontext_clear (GstXContext * xcontext);
static GstBuffer * gst_nvimageutil_nvimage_new (GstXContext * xcontext, GstElement * parent, guint fps_n, guint fps_d, gint bitrate, gboolean show_pointer, gint forcekeyframe, gint64 frame, gint64 ts);

GType
gst_meta_nvimage_api_get_type (void)
{
        static volatile GType type;
        static const gchar *tags[] = { "memory", NULL };

        if (g_once_init_enter (&type)) {
                GType _type = gst_meta_api_type_register ("GstMetaNVimageSrcAPI", tags);
                g_once_init_leave (&type, _type);
        }
        return type;
}

static gboolean
gst_meta_nvimage_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
        GstMetaNVimage *emeta = (GstMetaNVimage *) meta;

        emeta->width = 0;
        emeta->height = 0;
        emeta->size = 0;
        emeta->data = 0;

        return TRUE;
}

const GstMetaInfo *
gst_meta_nvimage_get_info (void)
{
        static const GstMetaInfo *meta_nvimage_info = NULL;

        if (g_once_init_enter (&meta_nvimage_info)) {
                const GstMetaInfo *meta =
                        gst_meta_register (gst_meta_nvimage_api_get_type (), "GstMetaNVimageSrc",
                                sizeof (GstMetaNVimage), (GstMetaInitFunction) gst_meta_nvimage_init,
                                (GstMetaFreeFunction) NULL, (GstMetaTransformFunction) NULL);
                g_once_init_leave (&meta_nvimage_info, meta);
        }
        return meta_nvimage_info;
}

static void*
worker_thread(void *arg) {
        GstXContext *xcontext = (GstXContext *)(arg);
        gboolean retb;
        GstBuffer *buf;
        while(!xcontext->finish) {
                pthread_mutex_lock(&xcontext->mutex_in);
                if(! xcontext->funcdata.inputvalid) {
                        pthread_cond_wait(&xcontext->cond_in, &xcontext->mutex_in);
                }
                xcontext->funcdata.inputvalid = 0;                
                if(xcontext->finish) {
                        pthread_mutex_unlock(&xcontext->mutex_in);
                        return NULL;
                }
                switch(xcontext->funcdata.function) {
                        case 1:
                                retb = nvimageutil_xcontext_get(xcontext, xcontext->funcdata.args[0].parent, 
                                                                        xcontext->funcdata.args[1].display_name);
                                xcontext->funcdata.retval.b = retb;
                                xcontext->funcdata.retvalid = 1;
                                pthread_mutex_lock(&xcontext->mutex_out);
                                pthread_cond_broadcast(&xcontext->cond_out);
                                pthread_mutex_unlock(&xcontext->mutex_out);
                                break;
                        case 2:
                                nvimageutil_xcontext_clear(xcontext);
                                xcontext->funcdata.retvalid = 1;
                                pthread_mutex_lock(&xcontext->mutex_out);
                                pthread_cond_broadcast(&xcontext->cond_out);
                                pthread_mutex_unlock(&xcontext->mutex_out);
                                pthread_mutex_unlock(&xcontext->mutex_in);
                                return NULL;
                        case 3:
                                buf = gst_nvimageutil_nvimage_new(xcontext, xcontext->funcdata.args[0].parent,
                                                                        xcontext->funcdata.args[1].fps_n, xcontext->funcdata.args[2].fps_d,
                                                                        xcontext->funcdata.args[3].bitrate, xcontext->funcdata.args[4].show_pointer,
                                                                        xcontext->funcdata.args[5].forcekeyframe,
                                                                        xcontext->funcdata.args[6].frame,
                                                                        xcontext->funcdata.args[7].ts);
                                pthread_mutex_lock(&xcontext->mutex_out);
                                xcontext->funcdata.retvalid = 1;
                                xcontext->funcdata.retval.buf = buf;                                
                                pthread_cond_broadcast(&xcontext->cond_out);
                                pthread_mutex_unlock(&xcontext->mutex_out);
                                break;
                } 
                pthread_mutex_unlock(&xcontext->mutex_in);
        }
        return NULL;
}

static void
worker_init(GstXContext *xcontext) {
        xcontext->finish = 0;
        pthread_mutex_init(&xcontext->mutex_in, NULL);
        pthread_mutex_init(&xcontext->mutex_out, NULL);
        pthread_cond_init(&xcontext->cond_in, NULL);
        pthread_cond_init(&xcontext->cond_out, NULL);
        pthread_create(&xcontext->worker_tid, NULL, worker_thread, xcontext);
}


GstXContext *
nvimageutil_xcontext_get_r(GstElement * parent, const gchar * display_name)
{
        gboolean ret;
        GstXContext * xcontext = g_new0 (GstXContext, 1);
        worker_init(xcontext);        
        memset(&xcontext->funcdata, 0, sizeof(GstXThreadCall));
        pthread_mutex_lock(&xcontext->mutex_in);
        xcontext->funcdata.function = 1;
        xcontext->funcdata.args[0].parent = parent;
        xcontext->funcdata.args[1].display_name = display_name;
        xcontext->funcdata.retvalid = 0;
        xcontext->funcdata.inputvalid = 1;
        pthread_cond_signal(&xcontext->cond_in);
        pthread_mutex_unlock(&xcontext->mutex_in);
        pthread_mutex_lock(&xcontext->mutex_out);
        if(xcontext->funcdata.retvalid == 0) {
                pthread_cond_wait(&xcontext->cond_out, &xcontext->mutex_out);
        }
        ret = xcontext->funcdata.retval.b;
        pthread_mutex_unlock(&xcontext->mutex_out);

        if(!ret) {
                memset(&xcontext->funcdata, 0, sizeof(GstXThreadCall));
                xcontext->finish = 1;
                pthread_cond_signal(&xcontext->cond_in);
                pthread_join(xcontext->worker_tid, NULL);
                return NULL;
        }
        return xcontext;
}

void
nvimageutil_xcontext_clear_r (GstXContext * xcontext)
{
        memset(&xcontext->funcdata, 0, sizeof(GstXThreadCall));
        pthread_mutex_lock(&xcontext->mutex_in);
        xcontext->funcdata.function = 2;
        xcontext->funcdata.inputvalid = 1;
        pthread_mutex_unlock(&xcontext->mutex_in);
        pthread_cond_signal(&xcontext->cond_in);
        pthread_mutex_lock(&xcontext->mutex_out);
        if(xcontext->funcdata.retvalid == 0) {
                pthread_cond_wait(&xcontext->cond_out, &xcontext->mutex_out);
        }
        pthread_mutex_unlock(&xcontext->mutex_out);
        g_free (xcontext);
}

GstBuffer *
gst_nvimageutil_nvimage_new_r (GstXContext * xcontext, GstElement * parent, guint fps_n, guint fps_d, gint bitrate, gboolean show_pointer, gint forcekeyframe, gint64 frame, gint64 ts) {
        GstBuffer *ret;
        memset(&xcontext->funcdata, 0, sizeof(GstXThreadCall));
        pthread_mutex_lock(&xcontext->mutex_in);
        xcontext->funcdata.function = 3;
        xcontext->funcdata.args[0].parent = parent;
        xcontext->funcdata.args[1].fps_n = fps_n;
        xcontext->funcdata.args[2].fps_d = fps_d;
        xcontext->funcdata.args[3].bitrate = bitrate;
        xcontext->funcdata.args[4].show_pointer = show_pointer;
        xcontext->funcdata.args[5].forcekeyframe = forcekeyframe;
        xcontext->funcdata.args[6].frame = frame;
        xcontext->funcdata.args[7].ts = ts;
        xcontext->funcdata.retvalid = 0;
        xcontext->funcdata.inputvalid = 1;
        pthread_mutex_unlock(&xcontext->mutex_in);
        pthread_cond_signal(&xcontext->cond_in);
        pthread_mutex_lock(&xcontext->mutex_out);
        if(xcontext->funcdata.retvalid == 0) {
                pthread_cond_wait(&xcontext->cond_out, &xcontext->mutex_out);
        }
        ret = xcontext->funcdata.retval.buf;
        pthread_mutex_unlock(&xcontext->mutex_out);
        return ret;
}

/* This function gets the X Display and global info about it. Everything is
   stored in our object and will be cleaned when the object is disposed. Note
   here that caps for supported format are generated without any window or
   image creation */
static gboolean
nvimageutil_xcontext_get (GstXContext *xcontext, GstElement * parent, const gchar * display_name)
{
        gint n;
        GLXFBConfig *fbconfigs;
        gint res;

        int attribs[] = {
                GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT | GLX_WINDOW_BIT,
                GLX_BIND_TO_TEXTURE_RGBA_EXT, 1,
                GLX_BIND_TO_TEXTURE_TARGETS_EXT, GLX_TEXTURE_2D_BIT_EXT,
                None
        };


        xcontext->disp = XOpenDisplay (display_name);
        GST_DEBUG_OBJECT (parent, "opened display %p", xcontext->disp);
        if (!xcontext->disp) {
                g_free (xcontext);
                g_error ("Cannot open display");
                return FALSE;
        }
        xcontext->screen = DefaultScreenOfDisplay (xcontext->disp);

        xcontext->width = WidthOfScreen (xcontext->screen);
        xcontext->height = HeightOfScreen (xcontext->screen);

        fbconfigs = glXChooseFBConfig(xcontext->disp, DefaultScreen(xcontext->disp), attribs, &n);

        if (!fbconfigs) {
                XCloseDisplay (xcontext->disp);
                g_error ("Cannot get fbconfigs");
                return FALSE;
        }

        xcontext->glxctx = glXCreateNewContext(xcontext->disp, fbconfigs[0], GLX_RGBA_TYPE, None, True);

        if (xcontext->glxctx == None) {
                XFree(fbconfigs);
                XCloseDisplay (xcontext->disp);
                g_error ("Cannot create new glx context");
                return FALSE;
        }

        xcontext->pixmap = XCreatePixmap(xcontext->disp, XDefaultRootWindow(xcontext->disp), 
                                        1, 1, DisplayPlanes(xcontext->disp, XDefaultScreen(xcontext->disp)));

        if (xcontext->pixmap == None) {
                glXDestroyContext(xcontext->disp, xcontext->glxctx);
                XFree(fbconfigs);
                XCloseDisplay (xcontext->disp);
                g_error ("Cannot create pixmap");
                return FALSE;
        }

        xcontext->glxpixmap = glXCreatePixmap(xcontext->disp, fbconfigs[0], xcontext->pixmap, NULL);

        if (xcontext->glxpixmap == None) {
                XFreePixmap(xcontext->disp, xcontext->pixmap);
                glXDestroyContext(xcontext->disp, xcontext->glxctx);
                XFree(fbconfigs);
                XCloseDisplay (xcontext->disp);
                g_error ("Cannot create glx pixmap");
                return FALSE;
        }

        res = glXMakeCurrent(xcontext->disp, xcontext->glxpixmap, xcontext->glxctx);
        if (!res) {
                glXDestroyPixmap(xcontext->disp, xcontext->glxpixmap);
                XFreePixmap(xcontext->disp, xcontext->pixmap);
                glXDestroyContext(xcontext->disp, xcontext->glxctx);
                XFree(fbconfigs);
                XCloseDisplay (xcontext->disp);
                g_error ("Cannot set current context");
                return FALSE;
        }

        xcontext->fbconfig = fbconfigs[0];

        XFree(fbconfigs);

        xcontext->fps_n = 30;
        xcontext->fps_d = 1;
        xcontext->bitrate = 2000000;
        xcontext->goplen = 10;
        xcontext->show_pointer = 0;

        if (!nvimageutil_fbccontext_get(xcontext)) {
                nvimageutil_xcontext_clear(xcontext);
                return FALSE;
        }

        return TRUE;
}

/* This function cleans the X context. Closing the Display and unrefing the
   caps for supported formats. */
static void
nvimageutil_xcontext_clear (GstXContext * xcontext)
{
        g_return_if_fail (xcontext != NULL);

        nvimageutil_fbccontext_clear(xcontext);

        glXMakeCurrent(xcontext->disp, 0, NULL);
        glXDestroyPixmap(xcontext->disp, xcontext->glxpixmap);
        XFreePixmap(xcontext->disp, xcontext->pixmap);
        glXDestroyContext(xcontext->disp, xcontext->glxctx);
        XCloseDisplay (xcontext->disp);
}

static gboolean
nvimageutil_fbccontext_get(GstXContext *xcontext)
{
        NVFBCSTATUS                             fbcStatus;
        NVFBC_CREATE_HANDLE_PARAMS              createHandleParams;
        NVFBC_GET_STATUS_PARAMS                 statusParams;
        NVFBC_SIZE                              frameSize = { 0, 0};
        NVFBC_CREATE_CAPTURE_SESSION_PARAMS     createCaptureParams;
        NVENCSTATUS                             encStatus;
        NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS    encodeSessionParams;
        GUID                                    encodeGuid;
        NV_ENC_PRESET_CONFIG                    presetConfig;
        NV_ENC_INITIALIZE_PARAMS                initParams;
        NV_ENC_CREATE_BITSTREAM_BUFFER          bitstreamBufferParams;


        xcontext->pFn.dwVersion = NVFBC_VERSION;

        fbcStatus = NvFBCCreateInstance(&xcontext->pFn);
        if (fbcStatus != NVFBC_SUCCESS) {
                g_error ("Cannot create FBC instance %d", fbcStatus);
                return FALSE;
        }

        memset(&createHandleParams, 0, sizeof(createHandleParams));

        createHandleParams.dwVersion                 = NVFBC_CREATE_HANDLE_PARAMS_VER;
        createHandleParams.bExternallyManagedContext = NVFBC_TRUE;
        createHandleParams.glxCtx                    = xcontext->glxctx;
        createHandleParams.glxFBConfig               = xcontext->fbconfig;

        fbcStatus = xcontext->pFn.nvFBCCreateHandle(&xcontext->fbcHandle, &createHandleParams);
        if (fbcStatus != NVFBC_SUCCESS) {
                g_error ("Cannot create FBC handle %d", fbcStatus);
                return FALSE;
        }

        memset(&statusParams, 0, sizeof(statusParams));

        statusParams.dwVersion = NVFBC_GET_STATUS_PARAMS_VER;

        fbcStatus = xcontext->pFn.nvFBCGetStatus(xcontext->fbcHandle, &statusParams);
        if (fbcStatus != NVFBC_SUCCESS) {
                g_error ("Cannot get FBC status %d", fbcStatus);
                return FALSE;
        }

        frameSize.w = statusParams.screenSize.w;
        frameSize.h = statusParams.screenSize.h;
        frameSize.w = (frameSize.w + 3) & ~3;

        xcontext->width = frameSize.w;
        xcontext->height = frameSize.h;

        memset(&createCaptureParams, 0, sizeof(createCaptureParams));

        createCaptureParams.dwVersion                   = NVFBC_CREATE_CAPTURE_SESSION_PARAMS_VER;
        createCaptureParams.eCaptureType                = NVFBC_CAPTURE_TO_GL;
        // FIX: Disable cursor to support Direct Capture
        createCaptureParams.bWithCursor                 = NVFBC_FALSE;  // xcontext->show_pointer;
        createCaptureParams.frameSize                   = frameSize;
        createCaptureParams.eTrackingType               = NVFBC_TRACKING_SCREEN;
        createCaptureParams.bDisableAutoModesetRecovery = NVFBC_TRUE;
        // NvFBC settings for accurate framerate
        uint32_t target_fps = (xcontext->fps_n > 0 && xcontext->fps_d > 0) ? 
                              (xcontext->fps_n / xcontext->fps_d) : 60;
        // FIX: Push Model is REQUIRED for Direct Capture and minimal latency!
        uint32_t sampling_ms = (1000 / target_fps);  // Ignored when Push Model = TRUE
        createCaptureParams.dwSamplingRateMs = sampling_ms;
        createCaptureParams.bPushModel = NVFBC_TRUE;   // Push model for Direct Capture!
        createCaptureParams.bAllowDirectCapture = NVFBC_TRUE;  // Direct capture for performance
        
        g_debug("NvFBC capture: target_fps=%d, dwSamplingRateMs=%d, bPushModel=%d", 
                  target_fps, sampling_ms, createCaptureParams.bPushModel);

        fbcStatus = xcontext->pFn.nvFBCCreateCaptureSession(xcontext->fbcHandle, &createCaptureParams);
        
        if (fbcStatus != NVFBC_SUCCESS) {
                g_error ("Cannot create FBC session %d", fbcStatus);
                return FALSE;
        }

        xcontext->setupParams.dwVersion     = NVFBC_TOGL_SETUP_PARAMS_VER;
        xcontext->setupParams.eBufferFormat = NVFBC_BUFFER_FORMAT_NV12;

        fbcStatus = xcontext->pFn.nvFBCToGLSetUp(xcontext->fbcHandle, &xcontext->setupParams);
        if (fbcStatus != NVFBC_SUCCESS) {
                g_error ("Cannot setup FBC GL %d", fbcStatus);
                return FALSE;
        }

        xcontext->pEncFn.version = NV_ENCODE_API_FUNCTION_LIST_VER;

        encStatus = NvEncodeAPICreateInstance(&xcontext->pEncFn);
        if (encStatus != NV_ENC_SUCCESS) {
                g_error ("Cannot create NVENC instance %d", encStatus);
                return FALSE;
        }

        memset(&encodeSessionParams, 0, sizeof(encodeSessionParams));

        encodeSessionParams.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
        encodeSessionParams.apiVersion = NVENCAPI_VERSION;
        encodeSessionParams.deviceType = NV_ENC_DEVICE_TYPE_OPENGL;

        encStatus = xcontext->pEncFn.nvEncOpenEncodeSessionEx(&encodeSessionParams, &xcontext->encoder);
        if (encStatus != NV_ENC_SUCCESS) {
                g_error ("Cannot open NVENC session %d", encStatus);
                return FALSE;
        }

        encodeGuid = NV_ENC_CODEC_H264_GUID;

        memset(&presetConfig, 0, sizeof(presetConfig));

        presetConfig.version = NV_ENC_PRESET_CONFIG_VER;
        presetConfig.presetCfg.version = NV_ENC_CONFIG_VER;
                encStatus = xcontext->pEncFn.nvEncGetEncodePresetConfig(xcontext->encoder,
                                                                encodeGuid,
                                                                NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID,
                                                                &presetConfig);
        if (encStatus != NV_ENC_SUCCESS) {
                g_error ("Cannot get NVENC preset config %d", encStatus);
                return FALSE;
        }

        presetConfig.presetCfg.rcParams.averageBitRate   = xcontext->bitrate;
        presetConfig.presetCfg.rcParams.maxBitRate       = xcontext->bitrate;
        presetConfig.presetCfg.rcParams.vbvBufferSize    = 0;
        presetConfig.presetCfg.rcParams.rateControlMode  = NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ;
        presetConfig.presetCfg.rcParams.zeroReorderDelay = 1;
        presetConfig.presetCfg.profileGUID               = NV_ENC_H264_PROFILE_HIGH_GUID;
        presetConfig.presetCfg.encodeCodecConfig.h264Config.repeatSPSPPS           = 0;
        presetConfig.presetCfg.encodeCodecConfig.h264Config.outputAUD              = 1;
        presetConfig.presetCfg.encodeCodecConfig.h264Config.outputPictureTimingSEI = 1;
        presetConfig.presetCfg.encodeCodecConfig.h264Config.chromaFormatIDC        = 1;
        presetConfig.presetCfg.encodeCodecConfig.h264Config.level                  = NV_ENC_LEVEL_AUTOSELECT;
        // SMOOTHNESS: Reduce GOP for more frequent I-frames and smoothness
        uint32_t gop_size = (target_fps >= 60) ? 15 : (target_fps >= 30) ? 30 : 60;
        presetConfig.presetCfg.encodeCodecConfig.h264Config.idrPeriod              = gop_size;
	presetConfig.presetCfg.gopLength 					   = gop_size;
        
        // FORCE set VUI timing info for H.264 headers
        presetConfig.presetCfg.encodeCodecConfig.h264Config.h264VUIParameters.timingInfoPresentFlag = 1;
        presetConfig.presetCfg.encodeCodecConfig.h264Config.h264VUIParameters.numUnitInTicks = xcontext->fps_d;
        presetConfig.presetCfg.encodeCodecConfig.h264Config.h264VUIParameters.timeScale = xcontext->fps_n * 2;


	memset(&initParams, 0, sizeof(initParams));
        initParams.version = NV_ENC_INITIALIZE_PARAMS_VER;
        initParams.encodeGUID = encodeGuid;
        initParams.presetGUID = NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID;
        initParams.encodeConfig = &presetConfig.presetCfg;
        initParams.encodeWidth = frameSize.w;
        initParams.encodeHeight = frameSize.h;
        initParams.frameRateNum = xcontext->fps_n;
        initParams.frameRateDen = xcontext->fps_d;
        initParams.enablePTD = 1;
        
        g_debug("NVENC encoder: frameRateNum=%d, frameRateDen=%d, target_fps=%d", 
                  xcontext->fps_n, xcontext->fps_d, target_fps);
        g_debug("NVENC GOP: gopLength=%d, idrPeriod=%d", 
                  presetConfig.presetCfg.gopLength, presetConfig.presetCfg.encodeCodecConfig.h264Config.idrPeriod);

        encStatus = xcontext->pEncFn.nvEncInitializeEncoder(xcontext->encoder, &initParams);
        if (encStatus != NV_ENC_SUCCESS) {
                g_error ("Cannot initialize NVENC encoder %d", encStatus);
                return FALSE;
        }

        xcontext->mapParams.version = NV_ENC_MAP_INPUT_RESOURCE_VER;

        for (gint i = 0; i < NVFBC_TOGL_TEXTURES_MAX; i++) {
                NV_ENC_REGISTER_RESOURCE         registerParams;
                NV_ENC_INPUT_RESOURCE_OPENGL_TEX texParams;

                if (!xcontext->setupParams.dwTextures[i]) {
                        break;
                }

                memset(&registerParams, 0, sizeof(registerParams));

                texParams.texture = xcontext->setupParams.dwTextures[i];
                texParams.target = xcontext->setupParams.dwTexTarget;

                registerParams.version = NV_ENC_REGISTER_RESOURCE_VER;
                registerParams.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_OPENGL_TEX;
                registerParams.width = frameSize.w;
                registerParams.height = frameSize.h;
                registerParams.pitch = frameSize.w;
                registerParams.resourceToRegister = &texParams;
                registerParams.bufferFormat = NV_ENC_BUFFER_FORMAT_NV12;

                encStatus = xcontext->pEncFn.nvEncRegisterResource(xcontext->encoder, &registerParams);
                if (encStatus != NV_ENC_SUCCESS) {
                        g_error ("Cannot register NVENC resource %d", encStatus);
                        return FALSE;
                }

                xcontext->registeredResources[i] = registerParams.registeredResource;
        }

        memset(&bitstreamBufferParams, 0, sizeof(bitstreamBufferParams));
        bitstreamBufferParams.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;

        encStatus = xcontext->pEncFn.nvEncCreateBitstreamBuffer(xcontext->encoder, &bitstreamBufferParams);
        if (encStatus != NV_ENC_SUCCESS) {
                g_error ("Cannot create NVENC bitstream buffer %d", encStatus);
                return FALSE;
        }

        xcontext->outputBuffer = bitstreamBufferParams.bitstreamBuffer;

        xcontext->encParams.version = NV_ENC_PIC_PARAMS_VER;
        xcontext->encParams.inputWidth = frameSize.w;
        xcontext->encParams.inputHeight = frameSize.h;
        xcontext->encParams.inputPitch = frameSize.w;
        xcontext->encParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
        xcontext->encParams.outputBitstream = bitstreamBufferParams.bitstreamBuffer;

        //xcontext->out = fopen("/tmp/output.h264", "wb");

        return TRUE;

}

static gboolean
nvimageutil_fbccontext_clear(GstXContext *xcontext) {
        NVFBC_DESTROY_CAPTURE_SESSION_PARAMS destroyCaptureParams;
        NVFBC_DESTROY_HANDLE_PARAMS          destroyHandleParams;
        NVFBCSTATUS                          fbcStatus;
        NVENCSTATUS                          encStatus;

        if (xcontext->outputBuffer != NULL) {
                encStatus = xcontext->pEncFn.nvEncDestroyBitstreamBuffer(xcontext->encoder, xcontext->outputBuffer);
                if (encStatus != NV_ENC_SUCCESS) {
                        g_error("Cannot destroy bitstream buffer %d", encStatus);
                        return FALSE;
                }
        }
        for (gint i = 0; i < NVFBC_TOGL_TEXTURES_MAX; i++) {
                if (xcontext->registeredResources[i]) {
                        encStatus = xcontext->pEncFn.nvEncUnregisterResource(xcontext->encoder, xcontext->registeredResources[i]);
                        if (encStatus != NV_ENC_SUCCESS) {
                                g_error("Cannot unregister resource %d", encStatus);
                                return FALSE;
                        }
                        xcontext->registeredResources[i] = NULL;
                }
        }
        encStatus = xcontext->pEncFn.nvEncDestroyEncoder(xcontext->encoder);
        if (encStatus != NV_ENC_SUCCESS) {
                g_error("Cannot destroy encoder %d", encStatus);
                return FALSE;
        }

        memset(&destroyCaptureParams, 0, sizeof(destroyCaptureParams));
        destroyCaptureParams.dwVersion = NVFBC_DESTROY_CAPTURE_SESSION_PARAMS_VER;
        fbcStatus = xcontext->pFn.nvFBCDestroyCaptureSession(xcontext->fbcHandle, &destroyCaptureParams);
        if (fbcStatus != NVFBC_SUCCESS) {
                g_error("Cannot destroy capture session %d", fbcStatus);
                return FALSE;
        }

        memset(&destroyHandleParams, 0, sizeof(destroyHandleParams));
        destroyHandleParams.dwVersion = NVFBC_DESTROY_HANDLE_PARAMS_VER;
        fbcStatus = xcontext->pFn.nvFBCDestroyHandle(xcontext->fbcHandle, &destroyHandleParams);
        /*if (fbcStatus != NVFBC_SUCCESS) {
                g_error("Cannot destroy fbc handle %d", fbcStatus);
                return FALSE;
        }*/

        if(xcontext->out)
                fclose(xcontext->out);

        memset(&xcontext->pFn, 0, sizeof(xcontext->pFn));
        xcontext->fbcHandle = 0;
        memset(&xcontext->pEncFn, 0, sizeof(xcontext->pEncFn));
        xcontext->encoder = 0;
        memset(&xcontext->mapParams, 0, sizeof(xcontext->mapParams));
        memset(&xcontext->encParams, 0, sizeof(xcontext->encParams));
        memset(&xcontext->setupParams, 0, sizeof(xcontext->setupParams));
        return TRUE;
}

static gboolean
gst_nvimagesrc_buffer_dispose (GstBuffer * nvimage)
{
        GstElement *parent;
        GstMetaNVimage *meta;
        gboolean ret = TRUE;

        meta = GST_META_NVIMAGE_GET (nvimage);

        parent = meta->parent;
        if (parent == NULL) {
                g_warning ("NVimageSrcBuffer->nvimagesrc == NULL");
                return ret;
        }

        gst_object_unref (meta->parent);
        meta->parent = NULL;

        g_free(meta->data);
        meta->data = NULL;
        meta->size = 0;
        return ret;
}

/* This function handles GstNVimageSrcBuffer creation depending on XShm availability */
static GstBuffer *
gst_nvimageutil_nvimage_new (GstXContext * xcontext, GstElement * parent, guint fps_n, guint fps_d, gint bitrate, gboolean show_pointer, gint forcekeyframe, gint64 frame, gint64 ts) {
        GstBuffer                    *nvimage = NULL;
        GstMetaNVimage               *meta;
        NVFBC_TOGL_GRAB_FRAME_PARAMS grabParams;
        NVFBC_FRAME_GRAB_INFO        frameInfo;  // Добавляем для проверки Direct Capture
        NVFBCSTATUS                  fbcStatus;
        NVENCSTATUS                  encStatus;
        NV_ENC_LOCK_BITSTREAM        lockParams;
        gint                         i=0;

        if (xcontext->fps_n != fps_n ||
            xcontext->fps_d != fps_d ||
            xcontext->bitrate != bitrate ||
	    forcekeyframe ||
            xcontext->show_pointer != show_pointer) {
                xcontext->fps_n = fps_n;
                xcontext->fps_d = fps_d;
                xcontext->bitrate = bitrate;
                xcontext->show_pointer = show_pointer;
                g_debug ("Recreating FBCNVENC pipeline, parametres change: bitrate: %d, showpointer %d, fps: %f", bitrate, show_pointer, ((double)fps_n)/fps_d);
                if(!nvimageutil_fbccontext_clear(xcontext)) {
                        g_error("Cannot clear context. Flow error.");
                        return NULL;
                }
                if (!nvimageutil_fbccontext_get(xcontext)) {
                        gst_buffer_unref (nvimage);
                        g_error("Cannot create new context. Flow error.");
                        return NULL;
                }
        }

        nvimage = gst_buffer_new ();
        GST_MINI_OBJECT_CAST (nvimage)->dispose =
                (GstMiniObjectDisposeFunction) gst_nvimagesrc_buffer_dispose;

        meta = GST_META_NVIMAGE_ADD (nvimage);

restart:
        memset(&grabParams, 0, sizeof(grabParams));
        memset(&frameInfo, 0, sizeof(frameInfo));
        
        grabParams.dwVersion = NVFBC_TOGL_GRAB_FRAME_PARAMS_VER;
        grabParams.pFrameGrabInfo = &frameInfo;  // Add for getting Direct Capture info
        
        // BALANCE: Performance + stability + Push Model optimization
        grabParams.dwFlags = NVFBC_TOGL_GRAB_FLAGS_NOWAIT | 
                             NVFBC_TOGL_GRAB_FLAGS_FORCE_REFRESH |
                             NVFBC_TOGL_GRAB_FLAGS_NOWAIT_IF_NEW_FRAME_READY;  // Perfect for Push Model

        fbcStatus = xcontext->pFn.nvFBCToGLGrabFrame(xcontext->fbcHandle, &grabParams);
        
        // CHECK: Is Direct Capture active? (verbose mode only)
        if (fbcStatus == NVFBC_SUCCESS) {
                if (frameInfo.bDirectCapture) {
                        g_debug("✅ Direct Capture ACTIVE - minimal latency!");
                } else {
                        g_debug("⚠️  Direct Capture NOT active - check fullscreen/compositor");
                }
                g_debug("Frame timing info: dwWidth=%u, dwHeight=%u", 
                          frameInfo.dwWidth, frameInfo.dwHeight);
        }

        if (fbcStatus == NVFBC_ERR_MUST_RECREATE) {
                g_warning ("Recreating FBCNVENC pipeline, must recreate status.");
                if (!nvimageutil_fbccontext_clear(xcontext)) {
                        gst_buffer_unref (nvimage);
                        return NULL;
                }
                if (!nvimageutil_fbccontext_get(xcontext)) {
                        gst_buffer_unref (nvimage);
                        return NULL;
                }
                i++;
                if(i <= 3) {
                        goto restart;
                } else {
                        gst_buffer_unref (nvimage);
                        return NULL;
                }
        } else if (fbcStatus != NVFBC_SUCCESS) {
                gst_buffer_unref (nvimage);
                g_error("Cannot grab frame %d", fbcStatus);
                return NULL;
        }

        xcontext->mapParams.registeredResource = xcontext->registeredResources[grabParams.dwTextureIndex];
        encStatus = xcontext->pEncFn.nvEncMapInputResource(xcontext->encoder, &xcontext->mapParams);
        if (encStatus != NV_ENC_SUCCESS) {
                gst_buffer_unref (nvimage);
                g_error("Cannot Map input resource %d", encStatus);
                return NULL;
        }

        xcontext->encParams.inputBuffer = xcontext->mapParams.mappedResource;
        xcontext->encParams.bufferFmt = xcontext->mapParams.mappedBufferFmt;
        xcontext->encParams.frameIdx = frame;
        xcontext->encParams.inputDuration = (1000000000L*xcontext->fps_d)/xcontext->fps_n; 
        xcontext->encParams.inputTimeStamp = frame*xcontext->encParams.inputDuration;
        
        // DIAGNOSTICS: Non-blocking logging for smoothness analysis
        static gint64 last_timestamp = 0;
        gint64 current_time = g_get_real_time();
        gint64 target_interval_us = xcontext->encParams.inputDuration / 1000;  // ns -> μs
        
        if (last_timestamp > 0) {
            gint64 real_interval = current_time - last_timestamp;
            // DIAGNOSTICS: frame timing (verbose mode only)
            g_debug("Frame %ld: expected_interval=%ldμs, real_interval=%ldμs", 
                      frame, target_interval_us, real_interval);
        }
        last_timestamp = current_time;
        if(forcekeyframe) {
                g_debug("Forced keyframe");
                xcontext->encParams.encodePicFlags = NV_ENC_PIC_FLAG_FORCEIDR;
        } else {
                xcontext->encParams.encodePicFlags = 0;
        }

        encStatus = xcontext->pEncFn.nvEncEncodePicture(xcontext->encoder, &xcontext->encParams);

        if (encStatus != NV_ENC_SUCCESS) {
                gst_buffer_unref (nvimage);
                g_error("Cannot encode picture %d", encStatus);
                return NULL;
        }

        memset(&lockParams, 0, sizeof(lockParams));
        lockParams.version = NV_ENC_LOCK_BITSTREAM_VER;
        lockParams.outputBitstream = xcontext->outputBuffer;

        encStatus = xcontext->pEncFn.nvEncLockBitstream(xcontext->encoder, &lockParams);
        if (encStatus != NV_ENC_SUCCESS) {
                gst_buffer_unref (nvimage);
                g_error("Cannot lock bitstream %d", encStatus);
                return NULL;
        }

        meta->data = g_new(char, lockParams.bitstreamSizeInBytes);
        meta->size = lockParams.bitstreamSizeInBytes;
        meta->width = xcontext->encParams.inputWidth;
        meta->height = xcontext->encParams.inputHeight;
        memcpy(meta->data, lockParams.bitstreamBufferPtr, lockParams.bitstreamSizeInBytes);
        if(xcontext->out)
                fwrite(meta->data, 1, meta->size, xcontext->out);

        encStatus = xcontext->pEncFn.nvEncUnlockBitstream(xcontext->encoder, xcontext->outputBuffer);

        if (encStatus != NV_ENC_SUCCESS) {
                gst_buffer_unref (nvimage);
                g_free(meta->data);
                g_error("Cannot unlock bitstream %d", encStatus);
                return NULL;
        }

        encStatus = xcontext->pEncFn.nvEncUnmapInputResource(xcontext->encoder, xcontext->encParams.inputBuffer);

        if (encStatus != NV_ENC_SUCCESS) {
                gst_buffer_unref (nvimage);
                g_free(meta->data);
                g_error("Cannot unmap input resource %d", encStatus);
                return NULL;
        }

        gst_buffer_append_memory (nvimage, gst_memory_new_wrapped (GST_MEMORY_FLAG_NO_SHARE, meta->data,
                                        meta->size, 0, meta->size, NULL, NULL));

        /* Keep a ref to our src */
        meta->parent = gst_object_ref (parent);

        return nvimage;
}

/* This function destroys a GstNVimageBuffer handling XShm availability */
void
gst_nvimageutil_nvimage_destroy (GstXContext * xcontext, GstBuffer * nvimage)
{
        GstMetaNVimage *meta;

        meta = GST_META_NVIMAGE_GET (nvimage);

        /* We might have some buffers destroyed after changing state to NULL */
        if (!xcontext)
                goto beach;

        g_return_if_fail (nvimage != NULL);

        g_free(meta->data);
        meta->data = NULL;
        meta->size = 0;
beach:
        if (meta->parent) {
                /* Release the ref to our parent */
                gst_object_unref (meta->parent);
                meta->parent = NULL;
        }

        return;
}
