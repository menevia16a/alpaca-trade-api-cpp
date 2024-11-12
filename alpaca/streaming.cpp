#include "streaming.h"
#include "glog/logging.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include <uWebSockets/App.h>

namespace alpaca::stream {
    const std::string kAuthorizationStream = "authorization";
    const std::string kListeningStream = "listening";
    const std::string kTradeUpdatesStream = "trade_updates";
    const std::string kAccountUpdatesStream = "account_updates";

    std::string streamToString(const StreamType stream) {
        switch (stream) {
        case TradeUpdates:
            return "trade_updates";
        case AccountUpdates:
            return "account_updates";
        case UnknownStreamType:
            return "unknown";
        }

        return "unknown";  // Fallback to satisfy compiler
    }

    std::string MessageGenerator::authentication(const std::string& key_id, const std::string& secret_key) const {
        rapidjson::StringBuffer s;
        rapidjson::Writer<rapidjson::StringBuffer> writer(s);

        writer.StartObject();
        writer.Key("action");
        writer.String("authenticate");
        writer.Key("data");
        writer.StartObject();
        writer.Key("key_id");
        writer.String(key_id.c_str());
        writer.Key("secret_key");
        writer.String(secret_key.c_str());
        writer.EndObject();
        writer.EndObject();

        return s.GetString();
    }

    std::string MessageGenerator::listen(const std::set<StreamType>& streams) const {
        rapidjson::StringBuffer s;
        rapidjson::Writer<rapidjson::StringBuffer> writer(s);

        writer.StartObject();
        writer.Key("action");
        writer.String("listen");
        writer.Key("data");
        writer.StartObject();
        writer.Key("streams");
        writer.StartArray();

        for (const auto& stream : streams)
            writer.String(streamToString(stream).c_str());

        writer.EndArray();
        writer.EndObject();
        writer.EndObject();

        return s.GetString();
    }

    std::pair<Status, Reply> parseReply(const std::string& text) {
        Reply r;
        rapidjson::Document d;

        if (d.Parse(text.c_str()).HasParseError())
            return std::make_pair(Status(1, "Received parse error when deserializing reply JSON"), r);

        if (!d.IsObject())
            return std::make_pair(Status(1, "Deserialized valid JSON but it wasn't an object"), r);

        if (d.HasMember("stream") && d["stream"].IsString()) {
            auto stream = d["stream"].GetString();

            if (*stream == *kAuthorizationStream.c_str()) {
                r.reply_type = Authorization;

                return std::make_pair(Status(), r);
            }
            else if (*stream == *kListeningStream.c_str()) {
                r.reply_type = Listening;

                return std::make_pair(Status(), r);
            }
            else if (*stream == *kTradeUpdatesStream.c_str()) {
                r.reply_type = Update;
                r.stream_type = TradeUpdates;
            }
            else if (*stream == *kAccountUpdatesStream.c_str()) {
                r.reply_type = Update;
                r.stream_type = AccountUpdates;
            }
            else {
                std::ostringstream ss;

                ss << "Unknown stream string: " << stream;

                return std::make_pair(Status(1, ss.str()), r);
            }
        }
        else {
            return std::make_pair(Status(1, "Reply did not contain stream key"), r);
        }

        if (d.HasMember("data") && d["data"].IsObject()) {
            rapidjson::StringBuffer s;
            rapidjson::Writer<rapidjson::StringBuffer> writer(s);

            d["data"].Accept(writer);
            r.data = s.GetString();
        }

        return std::make_pair(Status(), r);
    }

    Status Handler::run(Environment& env) {
        if (!env.hasBeenParsed()) {
            if (auto status = env.parse(); !status.ok()) {
                return status;
            }
        }

        auto message_generator_ = MessageGenerator();
        auto authentication = message_generator_.authentication(env.getAPIKeyID(), env.getAPISecretKey());
        auto listen = message_generator_.listen({StreamType::TradeUpdates, StreamType::AccountUpdates});
        std::ostringstream ss;

        ss << "wss://" << env.getAPIBaseURL() << "/stream";

        std::string ws_url = ss.str();
        uWS::App().ws<Handler*>("/*", {
            .compression = uWS::SHARED_COMPRESSOR,
            .maxPayloadLength = 16 * 1024,
            .idleTimeout = 10,
            .open = [authentication](auto* ws) {
                DLOG(INFO) << "Received connection event, sending authenticate message";
                ws->send(authentication, uWS::OpCode::TEXT);
            },
            .message = [this, listen](auto* ws, std::string_view message, uWS::OpCode opCode) {
                auto parsed_reply = parseReply(std::string(message));

                if (auto status = parsed_reply.first; !status.ok()) {
                    LOG(ERROR) << "Error parsing stream reply: " << status.getMessage();

                    return;
                }

                auto reply = parsed_reply.second;

                if (reply.reply_type == ReplyType::Authorization) {
                    DLOG(INFO) << "Sending listen message: " << listen;
                    ws->send(listen, uWS::OpCode::TEXT);
                }
                else if (reply.reply_type == ReplyType::Listening)
                    DLOG(INFO) << "Received listening confirmation";
                else if (reply.reply_type == ReplyType::Update) {
                    DLOG(INFO) << "Received update message";

                    if (reply.stream_type == StreamType::TradeUpdates) {
                        DLOG(INFO) << "Received trade update";
                        on_trade_update_(reply.data);
                    }
                    else if (reply.stream_type == StreamType::AccountUpdates) {
                        DLOG(INFO) << "Received account update";
                        on_account_update_(reply.data);
                    }
                    else
                        LOG(WARNING) << "Received unknown stream type";
                }
                else
                    LOG(WARNING) << "Received unknown stream reply type";
            },
            .drain = [](auto* ws) {
                LOG(WARNING) << "WebSocket backpressure limit reached";
            },
            .close = [](auto* ws, int code, std::string_view message) {
                DLOG(INFO) << "Received disconnection event: " << message;
            }
        }).connect(ws_url, [](auto*, auto*) {}).run();

        return Status(1, "Stream connection terminated");
    }
} // namespace alpaca::stream
