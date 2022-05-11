/*
 * Copyright (c) 2022 anqi.huang@outlook.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <binder/ProcessState.h>
#include <binder/IPCThreadState.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <android/native_window.h>
#include <android/hardware_buffer.h>
#include <core/SkCanvas.h>
#include <core/SkBitmap.h>
#include <core/SkRegion.h>
#include <core/SkTypeface.h>
#include <cutils/memory.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/Surface.h>
#include <gui/ISurfaceComposer.h>
#include <gui/SurfaceControl.h>
#include <ui/DisplayConfig.h>
#include <ui/DisplayState.h>
#include "cutils/properties.h"
#include <ToolUtils.h>
#include <effects/Sk2DPathEffect.h>
#include <effects/SkColorMatrixFilter.h>
#include <effects/SkGradientShader.h>
#include <core/SkGraphics.h>
#include <effects/SkLayerDrawLooper.h>
#include <core/SkMaskFilter.h>
#include <utils/SkRandom.h>
#include <core/SkTextBlob.h>

#include "watermark.h"
#include "log.h"


// Unlike the variant in sk_tool_utils, this version positions the glyphs on a diagonal
static void add_to_text(SkTextBlobBuilder *builder, SkScalar x, SkScalar y, const char *text) {

    SkTDArray <uint16_t> glyphs;

    size_t len = strlen(text);
    glyphs.append(font.countText(text, len, SkTextEncoding::kUTF8));
    font.textToGlyphs(text, len, SkTextEncoding::kUTF8, glyphs.begin(), glyphs.count());

    const SkScalar advanceX = font.getSize() * 0.85f;
    const SkScalar advanceY = font.getSize() * 1.5f;

    SkTDArray <SkScalar> pos;
    for (unsigned i = 0; i < len; ++i) {
        *pos.append() = x + i * advanceX;
        *pos.append() = y + i * (advanceY / len);
    }
    const SkTextBlobBuilder::RunBuffer &run = builder->allocRunPos(font, glyphs.count());
    memcpy(run.glyphs, glyphs.begin(), glyphs.count() * sizeof(uint16_t));
    memcpy(run.pos, pos.begin(), len * sizeof(SkScalar) * 2);
}

static void draw_text(int dx, int dy) {
    LOGD("draw text, dx = %d, dy = %d", dx, dy);
    SkTextBlobBuilder builder;
    sk_sp <SkTextBlob> fBlob;

    SkScalar x = 0;
    SkScalar y = 0;

    canvas->save();
    canvas->translate(dx, dy);
    canvas->rotate(-60.0f);
    add_to_text(&builder, x, y, property_imei_1);
    fBlob = builder.make();
    canvas->drawTextBlob(fBlob, x, y, paint);
    canvas->translate(0, 60);
    add_to_text(&builder, x, y, property_imei_2);
    fBlob = builder.make();
    canvas->drawTextBlob(fBlob, x, y, paint);
    canvas->restore();
}

static inline SkImageInfo convertPixelFormat(const ANativeWindow_Buffer &buffer) {
    SkColorType colorType = kUnknown_SkColorType;
    SkAlphaType alphaType = kOpaque_SkAlphaType;
    switch (buffer.format) {
        case WINDOW_FORMAT_RGBA_8888:
            colorType = kN32_SkColorType;
            alphaType = kPremul_SkAlphaType;
            break;
        case WINDOW_FORMAT_RGBX_8888:
            colorType = kN32_SkColorType;
            alphaType = kOpaque_SkAlphaType;
            break;
        case AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT:
            colorType = kRGBA_F16_SkColorType;
            alphaType = kPremul_SkAlphaType;
            break;
        case WINDOW_FORMAT_RGB_565:
            colorType = kRGB_565_SkColorType;
            alphaType = kOpaque_SkAlphaType;
            break;
        default:
            break;
    }
    return SkImageInfo::Make(buffer.width, buffer.height, colorType, alphaType);
}


int main(int argc __attribute__((unused)), char **argv __attribute__((unused))) {
    setbuf(stdout, NULL);
    ProcessState::self()->startThreadPool();

    //get property watermark
    char persist_sys_watermark[BUFF];
    property_get(PROPERTY_WATERMARK, persist_sys_watermark, PROPERTY_WATERMARK_DEFAULT);
    int enable_flag = atoi(persist_sys_watermark);
    LOGD("property watermark = %d", enable_flag);
    if (enable_flag != 1) {
        IPCThreadState::self()->joinThreadPool();
        IPCThreadState::self()->stopProcess();
    }

    //get property watermark
    char persist_sys_watermark_alpha[BUFF];
    property_get(PROPERTY_WATERMARK_ALPHA, persist_sys_watermark_alpha, PROPERTY_WATERMARK_ALPHA_DEFAULT);
    int alpha_flag = atoi(persist_sys_watermark_alpha);
    int alpha = atoi(PROPERTY_WATERMARK_ALPHA_DEFAULT);
    if (alpha_flag > 0 && alpha_flag <= 255) {
        alpha = alpha_flag;
    }
    LOGD("watermark alpha = %d", alpha);

    //get property fingerprint dev_prop
    char property_fingerprint[BUFF];
    property_get(PROPERTY_BUILD_FINGERPRINT, property_fingerprint, PROPERTY_BUILD_FINGERPRINT_DEFAULT);

    /**************************************init surface**************************************/
    // 1
    // get the client of SurfaceFlinger service.
    sp <SurfaceComposerClient> surfaceClient = new SurfaceComposerClient();
    status_t err = surfaceClient->initCheck();
    if (err != NO_ERROR) {
        ALOGE_IF(err, "SurfaceComposer::initCheck error: (%s) ", strerror(-err));
        return -1;
    }

    // 1.1
    // config display
    DisplayConfig dconfig;
    sp <IBinder> dtoken(SurfaceComposerClient::getInternalDisplayToken());
    status_t status_get_dconfig = SurfaceComposerClient::getActiveDisplayConfig(dtoken, &dconfig);
    if (status_get_dconfig) {
        return -1;
    }

    status_t status_get_displayState = SurfaceComposerClient::getDisplayState(dtoken, &displayState);
    if (status_get_displayState) {
        return -1;
    }
    LOGW("DisplayConfig: (%d,%d,%d).", dconfig.resolution.width, dconfig.resolution.height,
         displayState.orientation);

    int a = std::max(dconfig.resolution.width, dconfig.resolution.height);
    Rect displayRect(a, a);

    SurfaceComposerClient::Transaction().setDisplayProjection(dtoken, displayState.orientation, displayRect,
                                                              displayRect);

    // 2
    // create a native surface and get its SurfaceControl.
    sp <SurfaceControl> surfaceControl = surfaceClient->createSurface(
            String8("jos_watermark"), displayRect.getWidth(), displayRect.getHeight(), WINDOW_FORMAT_RGBA_8888);

    struct InputWindowInfo inputInfo;
    inputInfo.name = "jos_watermark#0";
    inputInfo.layoutParamsType |= InputWindowInfo::TYPE_SECURE_SYSTEM_OVERLAY;
    // 2.1
    // config properties of the surface we created
    SurfaceComposerClient::Transaction()
            .setLayer(surfaceControl, INT32_MAX - 1)
            .setInputWindowInfo(surfaceControl, inputInfo)
            .setPosition(surfaceControl, 0, 0)
            .show(surfaceControl)
            .apply(true);

    // 3
    // get surface handler from SurfaceControll
    sp <Surface> surface = surfaceControl->getSurface();
    ANativeWindow_Buffer outBuffer;
    // do real draw actions
    surface->lock(&outBuffer, NULL);
    /**************************************init surface**************************************/

    SkBitmap bitmap;
    ssize_t bpr = outBuffer.stride * bytesPerPixel(outBuffer.format);
    bitmap.installPixels(SkImageInfo::MakeN32Premul(outBuffer.width, outBuffer.height), outBuffer.bits, bpr);
    LOGW("outBuffer: (%d,%d).", outBuffer.width, outBuffer.height);
    bitmap.setInfo(convertPixelFormat(outBuffer), bpr);
    bitmap.setPixels(outBuffer.bits);

    canvas = new SkCanvas(bitmap);
    paint.setStrokeWidth(25);
    paint.setAntiAlias(true);
    paint.setARGB(255, 255, 0, 0);

    font.setSize(68);
    font.setSubpixel(true);
    font.setTypeface(SkTypeface::MakeFromFile("/system/fonts/DroidSans-Bold.ttf"));

    int retry = RETRY_TIMES;
    while (true || --retry > 0) {
        //get imei
        property_get(PROPERTY_IMEI1, property_imei_1, IMEI_UNKNOWN);
        property_get(PROPERTY_IMEI2, property_imei_2, IMEI_UNKNOWN);

        canvas->clear(SK_ColorTRANSPARENT);
        switch (displayState.orientation) {
            case ui::ROTATION_0:
            case ui::ROTATION_180:
                draw_text(0, 900);
                draw_text(0, 2180);
                draw_text(800, 900);
                draw_text(800, 2180);
                draw_text(1600, 900);
                draw_text(1600, 2180);

                break;
            case ui::ROTATION_90:
            case ui::ROTATION_270:
                draw_text(0, 900);
                draw_text(800, 900);
                draw_text(1600, 900);
                draw_text(0, 2180);
                draw_text(800, 2180);
                draw_text(1600, 2180);

                break;
        }

        surface->unlockAndPost();
        if (strcmp(property_imei_1, UNKNOWN) == 0 && strcmp(property_imei_2, UNKNOWN) == 0) {
            LOGE("retry read imei, imei1 = %s, imei2 = %s", property_imei_1, property_imei_2);
            sleep(5);
        } else {
            LOGD("success to read imei imei1 = %s, imei2 = %s", property_imei_1, property_imei_2);
            break;
        }
    }

    IPCThreadState::self()->joinThreadPool();
    IPCThreadState::self()->stopProcess();
    return 0;
}
