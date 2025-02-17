/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(SERVICE_WORKER)
#include "ServiceWorkerClients.h"

#include "JSDOMPromiseDeferred.h"
#include "JSServiceWorkerWindowClient.h"
#include "SWContextManager.h"
#include "ServiceWorker.h"
#include "ServiceWorkerGlobalScope.h"
#include "ServiceWorkerThread.h"
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

static inline void didFinishGetRequest(ServiceWorkerGlobalScope& scope, DeferredPromise& promise, std::optional<ServiceWorkerClientData>&& clientData)
{
    if (!clientData) {
        promise.resolve();
        return;
    }

    promise.resolve<IDLInterface<ServiceWorkerClient>>(ServiceWorkerClient::create(scope, WTFMove(*clientData)));
}

void ServiceWorkerClients::get(ScriptExecutionContext& context, const String& id, Ref<DeferredPromise>&& promise)
{
    auto serviceWorkerIdentifier = downcast<ServiceWorkerGlobalScope>(context).thread().identifier();

    callOnMainThread([promiseIdentifier = addPendingPromise(WTFMove(promise)), serviceWorkerIdentifier, id = id.isolatedCopy()] () {
        auto connection = SWContextManager::singleton().connection();
        connection->findClientByVisibleIdentifier(serviceWorkerIdentifier, id, [promiseIdentifier, serviceWorkerIdentifier] (auto&& clientData) {
            SWContextManager::singleton().postTaskToServiceWorker(serviceWorkerIdentifier, [promiseIdentifier, data = crossThreadCopy(WTFMove(clientData))] (auto& context) mutable {
                if (auto promise = context.clients().takePendingPromise(promiseIdentifier))
                    didFinishGetRequest(context, *promise, WTFMove(data));
            });
        });
    });
}


static inline void matchAllCompleted(ServiceWorkerGlobalScope& scope, DeferredPromise& promise, Vector<ServiceWorkerClientData>&& clientsData)
{
    auto clients = WTF::map(clientsData, [&] (auto&& clientData) {
        return ServiceWorkerClient::create(scope, WTFMove(clientData));
    });
    std::sort(clients.begin(), clients.end(), [&] (auto& a, auto& b) {
        return a->data().focusOrder > b->data().focusOrder;
    });
    promise.resolve<IDLSequence<IDLInterface<ServiceWorkerClient>>>(WTFMove(clients));
}

void ServiceWorkerClients::matchAll(ScriptExecutionContext& context, const ClientQueryOptions& options, Ref<DeferredPromise>&& promise)
{
    auto serviceWorkerIdentifier = downcast<ServiceWorkerGlobalScope>(context).thread().identifier();

    callOnMainThread([promiseIdentifier = addPendingPromise(WTFMove(promise)), serviceWorkerIdentifier, options] () mutable {
        auto connection = SWContextManager::singleton().connection();
        connection->matchAll(serviceWorkerIdentifier, options, [promiseIdentifier, serviceWorkerIdentifier] (auto&& clientsData) mutable {
            SWContextManager::singleton().postTaskToServiceWorker(serviceWorkerIdentifier, [promiseIdentifier, clientsData = crossThreadCopy(WTFMove(clientsData))] (auto& scope) mutable {
                if (auto promise = scope.clients().takePendingPromise(promiseIdentifier))
                    matchAllCompleted(scope, *promise, WTFMove(clientsData));
            });
        });
    });
}

void ServiceWorkerClients::openWindow(ScriptExecutionContext&, const String& url, Ref<DeferredPromise>&& promise)
{
    UNUSED_PARAM(url);
    promise->reject(Exception { NotSupportedError, "clients.openWindow() is not yet supported"_s });
}

void ServiceWorkerClients::claim(ScriptExecutionContext& context, Ref<DeferredPromise>&& promise)
{
    auto serviceWorkerIdentifier = downcast<ServiceWorkerGlobalScope>(context).thread().identifier();

    callOnMainThread([promiseIdentifier = addPendingPromise(WTFMove(promise)), serviceWorkerIdentifier] () mutable {
        auto connection = SWContextManager::singleton().connection();
        connection->claim(serviceWorkerIdentifier, [promiseIdentifier, serviceWorkerIdentifier](auto&& result) mutable {
            SWContextManager::singleton().postTaskToServiceWorker(serviceWorkerIdentifier, [promiseIdentifier, result = crossThreadCopy(WTFMove(result))](auto& scope) mutable {
                if (auto promise = scope.clients().takePendingPromise(promiseIdentifier)) {
                    DOMPromiseDeferred<void> pendingPromise { promise.releaseNonNull() };
                    pendingPromise.settle(WTFMove(result));
                }
            });
        });
    });
}

ServiceWorkerClients::PromiseIdentifier ServiceWorkerClients::addPendingPromise(Ref<DeferredPromise>&& promise)
{
    auto identifier = PromiseIdentifier::generateThreadSafe();
    m_pendingPromises.add(identifier, WTFMove(promise));
    return identifier;
}

RefPtr<DeferredPromise> ServiceWorkerClients::takePendingPromise(PromiseIdentifier identifier)
{
    return m_pendingPromises.take(identifier);
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
