/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "StreamClientConnection.h"

namespace IPC {

StreamClientConnection::StreamClientConnection(Connection& connection, size_t size)
    : m_connection(connection)
    , m_buffer(size)
{
    // Read starts from 0 with limit of 0 and reader sleeping.
    sharedClientOffset().store(ClientOffset::serverIsSleepingTag, std::memory_order_relaxed);
    // Write starts from 0 with a limit of the whole buffer.
    sharedClientLimit().store(static_cast<ClientLimit>(0), std::memory_order_relaxed);
}

void StreamClientConnection::setWakeUpSemaphore(IPC::Semaphore&& semaphore)
{
    m_wakeUpSemaphore = WTFMove(semaphore);
    wakeUpServer();
}

void StreamClientConnection::wakeUpServer()
{
    if (!m_wakeUpSemaphore)
        return;

    m_wakeUpSemaphore->signal();
    m_remainingMessageCountBeforeSendingWakeUp = 0;
}

void StreamClientConnection::deferredWakeUpServer()
{
    if (m_wakeUpMessageHysteresis)
        m_remainingMessageCountBeforeSendingWakeUp = m_wakeUpMessageHysteresis;
    else
        wakeUpServer();
}

void StreamClientConnection::decrementRemainingMessageCountBeforeSendingWakeUp()
{
    if (!m_remainingMessageCountBeforeSendingWakeUp)
        return;

    if (!--m_remainingMessageCountBeforeSendingWakeUp)
        wakeUpServer();
}

void StreamClientConnection::sendDeferredWakeUpMessageIfNeeded()
{
    if (!m_remainingMessageCountBeforeSendingWakeUp)
        return;

    wakeUpServer();
}

StreamConnectionBuffer& StreamClientConnection::bufferForTesting()
{
    return m_buffer;
}

}
