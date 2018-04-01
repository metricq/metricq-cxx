#include <dataheap2/source.hpp>

#include "log.hpp"

#include <dataheap2/datachunk.pb.h>

#include <amqpcpp.h>

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace dataheap2
{

Source::Source(const std::string& token) : Connection(token), data_handler_(io_service)
{
    register_management_callback("discover", [token](const json&) {
        json response;
        response["alive"] = true;
        return response;
    });
}

Source::~Source()
{
}

void Source::setup_complete()
{
    rpc("source.register", [this](const auto& config) { config_callback(config); });
}

void Source::send(const std::string& id, const DataChunk& dc)
{
    data_channel_->publish(data_exchange_, id, dc.SerializeAsString());
}

void Source::send(const std::string& id, TimeValue tv)
{
    // TODO evaluate optimization of string construction
    data_channel_->publish(data_exchange_, id, DataChunk(tv).SerializeAsString());
}

void Source::config_callback(const nlohmann::json& config)
{
    log::debug("start parsing config");
    if (data_connection_)
    {
        if (config["dataServerAddress"] != data_server_address_)
        {
            log::fatal("changing dataServerAddress on the fly is not currently supported");
            std::abort();
        }
        if (config["dataExchange"] != data_exchange_)
        {
            log::fatal("changing dataQueue on the fly is not currently supported");
            std::abort();
        }
    }

    data_server_address_ = config["dataServerAddress"];
    data_exchange_ = config["dataExchange"];

    data_connection_ =
        std::make_unique<AMQP::TcpConnection>(&data_handler_, AMQP::Address(data_server_address_));
    data_channel_ = std::make_unique<AMQP::TcpChannel>(data_connection_.get());
    data_channel_->onError([](const char* message) {
        // report error
        log::error("data channel error: {}", message);
    });

    if (config.find("config") != config.end())
    {
        source_config_callback(config["config"]);
    }
    ready_callback();
}

void SourceMetric::flush()
{
    source_.send(id_, chunk_);
    chunk_.clear_time_delta();
    chunk_.clear_value();
    previous_timestamp_ = 0;
}

void SourceMetric::send(TimeValue tv)
{
    chunk_.add_time_delta(tv.time.time_since_epoch().count() - previous_timestamp_);
    previous_timestamp_ = tv.time.time_since_epoch().count();
    chunk_.add_value(tv.value);

    assert(chunk_.time_delta_size() == chunk_.value_size());
    if (chunk_size_ && chunk_.time_delta_size() == chunk_size_)
    {
        flush();
    }
}
} // namespace dataheap2
