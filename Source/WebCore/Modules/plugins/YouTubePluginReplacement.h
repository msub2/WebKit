/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "PluginReplacement.h"
#include <wtf/HashMap.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class YouTubeEmbedShadowElement;

class YouTubePluginReplacement final : public PluginReplacement {
public:
    static void registerPluginReplacement(PluginReplacementRegistrar);

    typedef HashMap<String, String> KeyValueMap;

    WEBCORE_EXPORT static String youTubeURLFromAbsoluteURL(const URL& srcURL, const String& srcString);

private:
    YouTubePluginReplacement(HTMLPlugInElement&, const Vector<String>& paramNames, const Vector<String>& paramValues);
    static Ref<PluginReplacement> create(HTMLPlugInElement&, const Vector<String>& paramNames, const Vector<String>& paramValues);
    static bool supportsMIMEType(const String&);
    static bool supportsFileExtension(const String&);
    static bool supportsURL(const URL&);
    static bool isEnabledBySettings(const Settings&);

    InstallResult installReplacement(ShadowRoot&) final;

    String youTubeURL(const String& rawURL);

    bool willCreateRenderer() final { return m_embedShadowElement; }
    RenderPtr<RenderElement> createElementRenderer(HTMLPlugInElement&, RenderStyle&&, const RenderTreePosition&) final;

    WeakPtr<HTMLPlugInElement> m_parentElement;
    RefPtr<YouTubeEmbedShadowElement> m_embedShadowElement;
    KeyValueMap m_attributes;
};

}
