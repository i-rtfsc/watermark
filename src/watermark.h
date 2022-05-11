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

#ifndef _WATERMARK_H
#define _WATERMARK_H

using namespace android;
using Transaction = SurfaceComposerClient::Transaction;

#define PROPERTY_BUILD_FINGERPRINT              "ro.build.fingerprint"
#define PROPERTY_BUILD_FINGERPRINT_DEFAULT      "null"

#define PROPERTY_WATERMARK                      "persist.sys.journeyOS.watermark"
#define PROPERTY_WATERMARK_DEFAULT              "1"

#define PROPERTY_WATERMARK_ALPHA                "persist.sys.journeyOS.watermark.alpha"
#define PROPERTY_WATERMARK_ALPHA_DEFAULT        "128"

#define PROPERTY_IMEI1                          "ro.ril.journeyOS.imei1"
#define PROPERTY_IMEI2                          "ro.ril.journeyOS.imei2"
#define IMEI_UNKNOWN                            "imei_unknown"

#define BUFF                                    128

#define RETRY_TIMES                             500
#define UNKNOWN                                 "unknown"

ui::DisplayState displayState;

// SkCanvas
SkCanvas *canvas;
// SkFont
SkFont font;
// SkPaint
SkPaint paint;

//get property imei1
char property_imei_1[BUFF];
//get property imei2
char property_imei_2[BUFF];

#endif //_WATERMARK_H

