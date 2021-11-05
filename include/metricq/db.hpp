// Copyright (c) 2018, ZIH,
// Technische Universitaet Dresden,
// Federal Republic of Germany
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright notice,
//       this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright notice,
//       this list of conditions and the following disclaimer in the documentation
//       and/or other materials provided with the distribution.
//     * Neither the name of metricq nor the names of its contributors
//       may be used to endorse or promote products derived from this software
//       without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#pragma once

#include <metricq/awaitable.hpp>
#include <metricq/history.pb.h>
#include <metricq/json.hpp>
#include <metricq/sink.hpp>

#include <vector>

namespace metricq
{
class Db : public Sink
{
public:
    Db(const std::string& token);

protected:
    virtual HistoryResponse on_history(const std::string& id, const HistoryRequest& content) = 0;
    // returns the metrics to subscribe to
    virtual json on_db_config(const json& config) = 0;
    virtual void on_db_ready() = 0;

private:
    void on_history(const AMQP::Message&);
    void setup_history_queue(const AMQP::QueueCallback& callback);
    awaitable<void> on_register_response(const json& response);
    // We keep this private to avoid confusion because this is done automatically through return of
    // on_db_config
    awaitable<void> db_subscribe(const json& metrics);

protected:
    awaitable<void> on_connected() override;

protected:
    std::string history_queue_;
    // Stored permanently to avoid expensive allocations
    HistoryRequest history_request_;
};
} // namespace metricq
