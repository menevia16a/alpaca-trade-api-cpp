#include "client.h"
#include "glog/logging.h"
#include "rapidjson/writer.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace alpaca {
const char* kJSONContentType = "application/json";

httplib::Headers headers(const Environment& environment) {
    return {
            {"User-Agent", "StockHound/1.0"},
            {"APCA-API-KEY-ID", environment.getAPIKeyID()},
            {"APCA-API-SECRET-KEY", environment.getAPISecretKey()},
            {"Accept", "*/*"},
            {"Connection", "close"},
            };
}

Client::Client(Environment& environment) {
    if (!environment.hasBeenParsed()) {
        if (auto s = environment.parse(); !s.ok()) {
            LOG(ERROR) << "Error parsing the environment: " << s.getMessage();
        }
    }
    environment_ = environment;
}

std::pair<Status, Account> Client::getAccount() const {
    Account account;

    httplib::Client client(environment_.getAPIBaseURL());
    client.set_follow_location(false);
    DLOG(INFO) << "Sending GET request to /v2/account";
    DLOG(INFO) << "Headers:";
    for (const auto& h : headers(environment_)) {
        DLOG(INFO) << h.first << ": " << h.second;
    }
    auto resp = client.Get("/v2/account", headers(environment_));
    if (!resp) {
        return std::make_pair(Status(1, "Call to /v2/account returned an empty response"), account);
    }

    if (resp->status != 200) {
        std::ostringstream ss;
        ss << "Call to /v2/account returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), account);
    }

    DLOG(INFO) << "Response from /v2/account: " << resp->body;
    return std::make_pair(account.fromJSON(resp->body), account);
}

std::pair<Status, AccountConfigurations> Client::getAccountConfigurations() const {
    AccountConfigurations account_configurations;

    httplib::Client client(environment_.getAPIBaseURL());
    client.set_follow_location(false);
    auto resp = client.Get("/v2/account/configurations", headers(environment_));
    if (!resp) {
        return std::make_pair(Status(1, "Call to /v2/account/configurations returned an empty response"),
                              account_configurations);
    }

    if (resp->status != 200) {
        std::ostringstream ss;
        ss << "Call to /v2/account/configurations returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), account_configurations);
    }

    DLOG(INFO) << "Response from /v2/account/configurations: " << resp->body;
    return std::make_pair(account_configurations.fromJSON(resp->body), account_configurations);
}

std::pair<Status, AccountConfigurations> Client::updateAccountConfigurations(const bool no_shorting,
                                                                             const std::string& dtbp_check,
                                                                             const std::string& trade_confirm_email,
                                                                             const bool suspend_trade) const {
    AccountConfigurations account_configurations;

    rapidjson::StringBuffer s;
    s.Clear();
    rapidjson::Writer<rapidjson::StringBuffer> writer(s);
    writer.StartObject();

    writer.Key("no_shorting");
    writer.Bool(no_shorting);

    writer.Key("dtbp_check");
    writer.String(dtbp_check.c_str());

    writer.Key("trade_confirm_email");
    writer.String(trade_confirm_email.c_str());

    writer.Key("suspend_trade");
    writer.Bool(suspend_trade);

    writer.EndObject();
    auto body = s.GetString();

    httplib::Client client(environment_.getAPIBaseURL());
    client.set_follow_location(false);
    auto resp = client.Patch("/v2/account/configurations", headers(environment_), body, kJSONContentType);
    if (!resp) {
        return std::make_pair(Status(1, "Call to /v2/account/configurations returned an empty response"),
                              account_configurations);
    }

    if (resp->status != 200) {
        std::ostringstream ss;
        ss << "Call to /v2/account/configurations returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), account_configurations);
    }

    DLOG(INFO) << "Response from /v2/account/configurations: " << resp->body;
    return std::make_pair(account_configurations.fromJSON(resp->body), account_configurations);
}

std::pair<Status, std::vector<std::variant<TradeActivity, NonTradeActivity>>> Client::getAccountActivity(
    const std::vector<std::string>& activity_types) const {
    std::vector<std::variant<TradeActivity, NonTradeActivity>> activities;

    std::string url = "/v2/account/activities";
    if (activity_types.size() > 0) {
        std::string query_string = "";
        for (auto i = 0; i < activity_types.size(); ++i) {
            query_string += activity_types[i];
            query_string += ",";
        }
        query_string.pop_back();
        url += "?activity_types=" + query_string;
    }

    httplib::Client client(environment_.getAPIBaseURL());
    client.set_follow_location(false);
    DLOG(INFO) << "Making request to: " << url;
    DLOG(INFO) << "Sending GET request to " << url;
    DLOG(INFO) << "Headers:";
    for (const auto& h : headers(environment_)) {
        DLOG(INFO) << h.first << ": " << h.second;
    }
    auto resp = client.Get(url.c_str(), headers(environment_));
    if (!resp) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an empty response";
        return std::make_pair(Status(1, ss.str()), activities);
    }

    if (resp->status != 200) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), activities);
    }

    DLOG(INFO) << "Response from " << url << ": " << resp->body;

    rapidjson::Document d;
    if (d.Parse(resp->body.c_str()).HasParseError()) {
        return std::make_pair(Status(1, "Received parse error when deserializing activities JSON"), activities);
    }
    for (auto& a : d.GetArray()) {
        std::string activity_type;
        if (a.HasMember("activity_type") && a["activity_type"].IsString()) {
            activity_type = a["activity_type"].GetString();
        } else {
            return std::make_pair(Status(1, "Activity didn't have activity_type attribute"), activities);
        }

        rapidjson::StringBuffer s;
        s.Clear();
        rapidjson::Writer<rapidjson::StringBuffer> writer(s);
        a.Accept(writer);

        if (activity_type == "FILL") {
            TradeActivity activity;
            if (auto status = activity.fromJSON(s.GetString()); !status.ok()) {
                return std::make_pair(status, activities);
            }
            activities.push_back(activity);
        } else {
            NonTradeActivity activity;
            if (auto status = activity.fromJSON(s.GetString()); !status.ok()) {
                return std::make_pair(status, activities);
            }
            activities.push_back(activity);
        }
    }

    return std::make_pair(Status(), activities);
}

std::pair<Status, Order> Client::getOrder(const std::string& id, const bool nested) const {
    Order order;

    auto url = "/v2/orders/" + id;
    if (nested) {
        url += "?nested=true";
    }

    httplib::Client client(environment_.getAPIBaseURL());
    client.set_follow_location(false);
    DLOG(INFO) << "Making request to: " << url;
    DLOG(INFO) << "Sending GET request to " << url;
    DLOG(INFO) << "Headers:";
    for (const auto& h : headers(environment_)) {
        DLOG(INFO) << h.first << ": " << h.second;
    }
    auto resp = client.Get(url.c_str(), headers(environment_));
    if (!resp) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an empty response";
        return std::make_pair(Status(1, ss.str()), order);
    }

    if (resp->status != 200) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), order);
    }

    DLOG(INFO) << "Response from " << url << ": " << resp->body;
    return std::make_pair(order.fromJSON(resp->body), order);
}

std::pair<Status, Order> Client::getOrderByClientOrderID(const std::string& client_order_id) const {
    Order order;

    auto url = "/v2/orders:by_client_order_id?client_order_id=" + client_order_id;

    httplib::Client client(environment_.getAPIBaseURL());
    client.set_follow_location(false);
    DLOG(INFO) << "Making request to: " << url;
    DLOG(INFO) << "Sending GET request to " << url;
    DLOG(INFO) << "Headers:";
    for (const auto& h : headers(environment_)) {
        DLOG(INFO) << h.first << ": " << h.second;
    }
    auto resp = client.Get(url.c_str(), headers(environment_));
    if (!resp) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an empty response";
        return std::make_pair(Status(1, ss.str()), order);
    }

    if (resp->status != 200) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), order);
    }

    DLOG(INFO) << "Response from " << url << ": " << resp->body;
    return std::make_pair(order.fromJSON(resp->body), order);
}

std::pair<Status, std::vector<Order>> Client::getOrders(const ActionStatus status,
                                                        const int limit,
                                                        const std::string& after,
                                                        const std::string& until,
                                                        const OrderDirection direction,
                                                        const bool nested) const {
    std::vector<Order> orders;

    httplib::Params params{
                           {"status", actionStatusToString(status)},
                           {"limit", std::to_string(limit)},
                           {"direction", orderDirectionToString(direction)},
                           };
    if (after != "") {
        params.insert({"after", after});
    }
    if (until != "") {
        params.insert({"until", until});
    }
    if (nested) {
        params.insert({"nested", "true"});
    }
    auto query_string = httplib::detail::params_to_query_str(params);
    httplib::Client client(environment_.getAPIBaseURL());
    client.set_follow_location(false);
    auto url = "/v2/orders?" + query_string;
    DLOG(INFO) << "Making request to: " << url;
    DLOG(INFO) << "Sending GET request to " << url;
    DLOG(INFO) << "Headers:";
    for (const auto& h : headers(environment_)) {
        DLOG(INFO) << h.first << ": " << h.second;
    }
    auto resp = client.Get(url.c_str(), headers(environment_));
    if (!resp) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an empty response";
        return std::make_pair(Status(1, ss.str()), orders);
    }

    if (resp->status != 200) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), orders);
    }

    DLOG(INFO) << "Response from " << url << ": " << resp->body;

    rapidjson::Document d;
    if (d.Parse(resp->body.c_str()).HasParseError()) {
        return std::make_pair(Status(1, "Received parse error when deserializing orders JSON"), orders);
    }
    for (auto& o : d.GetArray()) {
        Order order;
        rapidjson::StringBuffer s;
        s.Clear();
        rapidjson::Writer<rapidjson::StringBuffer> writer(s);
        o.Accept(writer);
        if (auto status = order.fromJSON(s.GetString()); !status.ok()) {
            return std::make_pair(status, orders);
        }
        orders.push_back(order);
    }

    return std::make_pair(Status(), orders);
}

std::pair<Status, Order> Client::submitOrder(const std::string& symbol,
                                             const int quantity,
                                             const OrderSide side,
                                             const OrderType type,
                                             const OrderTimeInForce tif,
                                             const std::string& limit_price,
                                             const std::string& stop_price,
                                             const bool extended_hours,
                                             const std::string& client_order_id,
                                             const OrderClass order_class,
                                             TakeProfitParams* take_profit_params,
                                             StopLossParams* stop_loss_params) const {
    Order order;

    rapidjson::StringBuffer s;
    s.Clear();
    rapidjson::Writer<rapidjson::StringBuffer> writer(s);
    writer.StartObject();

    writer.Key("symbol");
    writer.String(symbol.c_str());

    writer.Key("qty");
    writer.Int(quantity);

    writer.Key("side");
    writer.String(orderSideToString(side).c_str());

    writer.Key("type");
    writer.String(orderTypeToString(type).c_str());

    writer.Key("time_in_force");
    writer.String(orderTimeInForceToString(tif).c_str());

    if (limit_price != "") {
        writer.Key("limit_price");
        writer.String(limit_price.c_str());
    }

    if (stop_price != "") {
        writer.Key("stop_price");
        writer.String(stop_price.c_str());
    }

    if (extended_hours) {
        writer.Key("extended_hours");
        writer.Bool(extended_hours);
    }

    if (client_order_id != "") {
        writer.Key("client_order_id");
        writer.String(client_order_id.c_str());
    }

    if (order_class != OrderClass::Simple) {
        writer.Key("order_class");
        writer.String(orderClassToString(order_class).c_str());
    }

    if (take_profit_params != nullptr) {
        writer.Key("take_profit");
        writer.StartObject();
        if (take_profit_params->limitPrice != "") {
            writer.Key("limit_price");
            writer.String(take_profit_params->limitPrice.c_str());
        }
        writer.EndObject();
    }

    if (stop_loss_params != nullptr) {
        writer.Key("stop_loss");
        writer.StartObject();
        if (stop_loss_params->limitPrice != "") {
            writer.Key("limit_price");
            writer.String(stop_loss_params->limitPrice.c_str());
        }
        if (stop_loss_params->stopPrice != "") {
            writer.Key("stop_price");
            writer.String(stop_loss_params->stopPrice.c_str());
        }
        writer.EndObject();
    }

    writer.EndObject();
    auto body = s.GetString();

    DLOG(INFO) << "Sending request body to /v2/orders: " << body;

    httplib::Client client(environment_.getAPIBaseURL());
    client.set_follow_location(false);
    auto resp = client.Post("/v2/orders", headers(environment_), body, kJSONContentType);

    if (!resp) {
        return std::make_pair(Status(1, "Call to /v2/orders returned an empty response"), order);
    }

    if (resp->status != 200) {
        std::ostringstream ss;
        ss << "Call to /v2/orders returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), order);
    }

    DLOG(INFO) << "Response from /v2/orders: " << resp->body;

    return std::make_pair(order.fromJSON(resp->body), order);
}

std::pair<Status, Order> Client::replaceOrder(const std::string& id,
                                              const int quantity,
                                              const OrderTimeInForce tif,
                                              const std::string& limit_price,
                                              const std::string& stop_price,
                                              const std::string& client_order_id) const {
    Order order;

    rapidjson::StringBuffer s;
    s.Clear();
    rapidjson::Writer<rapidjson::StringBuffer> writer(s);
    writer.StartObject();

    writer.Key("qty");
    writer.Int(quantity);

    writer.Key("time_in_force");
    writer.String(orderTimeInForceToString(tif).c_str());

    if (limit_price != "") {
        writer.Key("limit_price");
        writer.String(limit_price.c_str());
    }

    if (stop_price != "") {
        writer.Key("stop_price");
        writer.String(stop_price.c_str());
    }

    if (client_order_id != "") {
        writer.Key("client_order_id");
        writer.String(client_order_id.c_str());
    }

    writer.EndObject();
    auto body = s.GetString();

    auto url = "/v2/orders/" + id;
    DLOG(INFO) << "Sending request body to " << url << ": " << body;

    httplib::Client client(environment_.getAPIBaseURL());
    client.set_follow_location(false);
    auto resp = client.Patch(url.c_str(), headers(environment_), body, kJSONContentType);
    if (!resp) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an empty response";
        return std::make_pair(Status(1, ss.str()), order);
    }

    if (resp->status != 200) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), order);
    }

    DLOG(INFO) << "Response from " << url << ": " << resp->body;

    return std::make_pair(order.fromJSON(resp->body), order);
}

std::pair<Status, std::vector<Order>> Client::cancelOrders() const {
    std::vector<Order> orders;

    httplib::Client client(environment_.getAPIBaseURL());
    client.set_follow_location(false);
    DLOG(INFO) << "Making request to: /v2/orders";
    auto resp = client.Delete("/v2/orders", headers(environment_));
    if (!resp) {
        return std::make_pair(Status(1, "Call to /v2/orders returned an empty response"), orders);
    }

    if (resp->status != 200 && resp->status != 207) {
        std::ostringstream ss;
        ss << "Call to /v2/orders returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), orders);
    }

    DLOG(INFO) << "Response from /v2/orders: " << resp->body;

    rapidjson::Document d;
    if (d.Parse(resp->body.c_str()).HasParseError()) {
        return std::make_pair(Status(1, "Received parse error when deserializing orders JSON"), orders);
    }
    for (auto& o : d.GetArray()) {
        Order order;
        rapidjson::StringBuffer s;
        s.Clear();
        rapidjson::Writer<rapidjson::StringBuffer> writer(s);
        o.Accept(writer);
        if (auto status = order.fromJSON(s.GetString()); !status.ok()) {
            return std::make_pair(status, orders);
        }
        orders.push_back(order);
    }

    return std::make_pair(Status(), orders);
}

std::pair<Status, Order> Client::cancelOrder(const std::string& id) const {
    Order order;

    httplib::Client client(environment_.getAPIBaseURL());
    client.set_follow_location(false);
    auto url = "/v2/orders/" + id;
    DLOG(INFO) << "Making request to: " << url;
    auto resp = client.Delete(url.c_str(), headers(environment_));
    if (!resp) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an empty response";
        return std::make_pair(Status(1, ss.str()), order);
    }

    if (resp->status == 204) {
        return getOrder(id);
    }

    if (resp->status != 200) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), order);
    }

    DLOG(INFO) << "Response from " << url << ": " << resp->body;

    return std::make_pair(order.fromJSON(resp->body), order);
}

std::pair<Status, std::vector<Position>> Client::getPositions() const {
    std::vector<Position> positions;

    httplib::Client client(environment_.getAPIBaseURL());
    client.set_follow_location(false);
    DLOG(INFO) << "Making request to: /v2/positions";
    auto resp = client.Get("/v2/positions", headers(environment_));
    if (!resp) {
        return std::make_pair(Status(1, "Call to /v2/positions returned an empty response"), positions);
    }

    if (resp->status != 200) {
        std::ostringstream ss;
        ss << "Call to /v2/positions returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), positions);
    }

    DLOG(INFO) << "Response from /v2/positions: " << resp->body;

    rapidjson::Document d;
    if (d.Parse(resp->body.c_str()).HasParseError()) {
        return std::make_pair(Status(1, "Received parse error when deserializing positions JSON"), positions);
    }
    for (auto& o : d.GetArray()) {
        Position position;
        rapidjson::StringBuffer s;
        s.Clear();
        rapidjson::Writer<rapidjson::StringBuffer> writer(s);
        o.Accept(writer);
        if (auto status = position.fromJSON(s.GetString()); !status.ok()) {
            return std::make_pair(status, positions);
        }
        positions.push_back(position);
    }

    return std::make_pair(Status(), positions);
}

std::pair<Status, Position> Client::getPosition(const std::string& symbol) const {
    Position position;

    auto url = "/v2/positions/" + symbol;

    httplib::Client client(environment_.getAPIBaseURL());
    client.set_follow_location(false);
    DLOG(INFO) << "Making request to: " << url;
    DLOG(INFO) << "Sending GET request to " << url;
    DLOG(INFO) << "Headers:";
    for (const auto& h : headers(environment_)) {
        DLOG(INFO) << h.first << ": " << h.second;
    }
    auto resp = client.Get(url.c_str(), headers(environment_));
    if (!resp) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an empty response";
        return std::make_pair(Status(1, ss.str()), position);
    }

    if (resp->status != 200) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), position);
    }

    DLOG(INFO) << "Response from " << url << ": " << resp->body;
    return std::make_pair(position.fromJSON(resp->body), position);
}

std::pair<Status, std::vector<Position>> Client::closePositions() const {
    std::vector<Position> positions;

    httplib::Client client(environment_.getAPIBaseURL());
    client.set_follow_location(false);
    DLOG(INFO) << "Making request to: /v2/positions";
    auto resp = client.Delete("/v2/orders", headers(environment_));
    if (!resp) {
        return std::make_pair(Status(1, "Call to /v2/positions returned an empty response"), positions);
    }

    if (resp->status != 200 && resp->status != 207) {
        std::ostringstream ss;
        ss << "Call to /v2/positions returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), positions);
    }

    DLOG(INFO) << "Response from /v2/positions: " << resp->body;

    rapidjson::Document doc;
    if (doc.Parse(resp->body.c_str()).HasParseError()) {
        return std::make_pair(Status(1, "Received parse error when deserializing positions JSON"), positions);
    }
    for (auto& o : doc.GetArray()) {
        Position position;
        rapidjson::StringBuffer s;
        s.Clear();
        rapidjson::Writer<rapidjson::StringBuffer> writer(s);
        o.Accept(writer);
        if (auto status = position.fromJSON(s.GetString()); !status.ok()) {
            return std::make_pair(status, positions);
        }
        positions.push_back(position);
    }

    return std::make_pair(Status(), positions);
}

std::pair<Status, Position> Client::closePosition(const std::string& symbol) const {
    Position position;

    httplib::Client client(environment_.getAPIBaseURL());
    client.set_follow_location(false);
    auto url = "/v2/positions/" + symbol;
    DLOG(INFO) << "Making request to: " << url;
    auto resp = client.Delete(url.c_str(), headers(environment_));
    if (!resp) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an empty response";
        return std::make_pair(Status(1, ss.str()), position);
    }

    if (resp->status == 204) {
        return getPosition(symbol);
    }

    if (resp->status != 200) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), position);
    }

    DLOG(INFO) << "Response from " << url << ": " << resp->body;

    return std::make_pair(position.fromJSON(resp->body), position);
}


std::pair<Status, std::vector<Asset>> Client::getAssets(const AssetClass asset_class, const ActionStatus asset_status, const std::string& exchange) const {
    std::vector<Asset> assets;
    std::string classParam = assetClassToString(asset_class);
    std::string statusParam = actionStatusToString(asset_status);

    std::string url = "/v2/assets?asset_class=" + classParam + "&status=" + statusParam + "&exchange=" + exchange + "&tradable=true";

    DLOG(INFO) << "Making request to: " << url;
    httplib::Client client(environment_.getAPIBaseURL());
    DLOG(INFO) << "Sending GET request to " << url;

    auto resp = client.Get(url.c_str(), headers(environment_));
    if (!resp) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an empty response";
        return std::make_pair(Status(1, ss.str()), assets);
    }

    if (resp->status != 200) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), assets);
    }

    DLOG(INFO) << "Response from " << url << ": " << resp->body;

    // Parse the JSON response
    rapidjson::Document doc;
    if (doc.Parse(resp->body.c_str()).HasParseError()) {
        return std::make_pair(Status(1, "JSON parse error"), assets);
    }

    // Append assets from the current response
    for (auto& o : doc.GetArray()) {
        Asset asset;
        rapidjson::StringBuffer s;
        s.Clear();
        rapidjson::Writer<rapidjson::StringBuffer> writer(s);
        o.Accept(writer);
        if (auto status = asset.fromJSON(s.GetString()); !status.ok()) {
            return std::make_pair(status, assets);
        }
        assets.push_back(asset);
    }

    return std::make_pair(Status(), assets);
}

std::pair<Status, Asset> Client::getAsset(const std::string& symbol) const {
    Asset asset;

    auto url = "/v2/assets/" + symbol;

    httplib::Client client(environment_.getAPIBaseURL());
    DLOG(INFO) << "Making request to: " << url;
    DLOG(INFO) << "Sending GET request to " << url;
    DLOG(INFO) << "Headers:";
    for (const auto& h : headers(environment_)) {
        DLOG(INFO) << h.first << ": " << h.second;
    }
    auto resp = client.Get(url.c_str(), headers(environment_));
    if (!resp) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an empty response";
        return std::make_pair(Status(1, ss.str()), asset);
    }

    if (resp->status != 200) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), asset);
    }

    DLOG(INFO) << "Response from " << url << ": " << resp->body;
    return std::make_pair(asset.fromJSON(resp->body), asset);
}

std::pair<Status, Clock> Client::getClock() const {
    Clock clock;

    httplib::Client client(environment_.getAPIBaseURL());
    client.set_follow_location(false);
    auto resp = client.Get("/v2/clock", headers(environment_));
    if (!resp) {
        return std::make_pair(Status(1, "Call to /v2/clock returned an empty response"), clock);
    }

    if (resp->status != 200) {
        std::ostringstream ss;
        ss << "Call to /v2/clock returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), clock);
    }

    DLOG(INFO) << "Response from /v2/clock: " << resp->body;
    return std::make_pair(clock.fromJSON(resp->body), clock);
}

std::pair<Status, std::vector<Date>> Client::getCalendar(const std::string& start, const std::string& end) const {
    std::vector<Date> dates;

    auto url = "/v2/calendar?start=" + start + "&end=" + end;
    httplib::Client client(environment_.getAPIBaseURL());
    client.set_follow_location(false);
    DLOG(INFO) << "Making request to: " << url;
    DLOG(INFO) << "Sending GET request to " << url;
    DLOG(INFO) << "Headers:";
    for (const auto& h : headers(environment_)) {
        DLOG(INFO) << h.first << ": " << h.second;
    }
    auto resp = client.Get(url.c_str(), headers(environment_));
    if (!resp) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an empty response";
        return std::make_pair(Status(1, ss.str()), dates);
    }

    if (resp->status != 200) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), dates);
    }

    DLOG(INFO) << "Response from " << url << ": " << resp->body;

    rapidjson::Document d;
    if (d.Parse(resp->body.c_str()).HasParseError()) {
        return std::make_pair(Status(1, "Received parse error when deserializing calendar JSON"), dates);
    }
    for (auto& o : d.GetArray()) {
        Date date;
        rapidjson::StringBuffer s;
        s.Clear();
        rapidjson::Writer<rapidjson::StringBuffer> writer(s);
        o.Accept(writer);
        if (auto status = date.fromJSON(s.GetString()); !status.ok()) {
            return std::make_pair(status, dates);
        }
        dates.push_back(date);
    }

    return std::make_pair(Status(), dates);
}

std::pair<Status, std::vector<Watchlist>> Client::getWatchlists() const {
    std::vector<Watchlist> watchlists;

    httplib::Client client(environment_.getAPIBaseURL());
    client.set_follow_location(false);
    DLOG(INFO) << "Making request to: /v2/watchlists";
    auto resp = client.Get("/v2/watchlists", headers(environment_));
    if (!resp) {
        return std::make_pair(Status(1, "Call to /v2/watchlists returned an empty response"), watchlists);
    }

    if (resp->status != 200) {
        std::ostringstream ss;
        ss << "Call to /v2/watchlists returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), watchlists);
    }

    DLOG(INFO) << "Response from /v2/watchlists: " << resp->body;

    rapidjson::Document d;
    if (d.Parse(resp->body.c_str()).HasParseError()) {
        return std::make_pair(Status(1, "Received parse error when deserializing watchlists JSON"), watchlists);
    }
    for (auto& o : d.GetArray()) {
        Watchlist watchlist;
        rapidjson::StringBuffer s;
        s.Clear();
        rapidjson::Writer<rapidjson::StringBuffer> writer(s);
        o.Accept(writer);
        if (auto status = watchlist.fromJSON(s.GetString()); !status.ok()) {
            return std::make_pair(status, watchlists);
        }
        watchlists.push_back(watchlist);
    }

    return std::make_pair(Status(), watchlists);
}

std::pair<Status, Watchlist> Client::getWatchlist(const std::string& id) const {
    Watchlist watchlist;

    auto url = "/v2/watchlists/" + id;
    httplib::Client client(environment_.getAPIBaseURL());
    client.set_follow_location(false);
    DLOG(INFO) << "Making request to: " << url;
    DLOG(INFO) << "Sending GET request to " << url;
    DLOG(INFO) << "Headers:";
    for (const auto& h : headers(environment_)) {
        DLOG(INFO) << h.first << ": " << h.second;
    }
    auto resp = client.Get(url.c_str(), headers(environment_));
    if (!resp) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an empty response";
        return std::make_pair(Status(1, ss.str()), watchlist);
    }

    if (resp->status != 200) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), watchlist);
    }

    DLOG(INFO) << "Response from " << url << ": " << resp->body;
    return std::make_pair(watchlist.fromJSON(resp->body), watchlist);
}

std::pair<Status, Watchlist> Client::createWatchlist(const std::string& name,
                                                     const std::vector<std::string>& symbols) const {
    Watchlist watchlist;

    rapidjson::StringBuffer s;
    s.Clear();
    rapidjson::Writer<rapidjson::StringBuffer> writer(s);
    writer.StartObject();

    writer.Key("name");
    writer.String(name.c_str());

    writer.Key("symbols");
    writer.StartArray();
    for (const auto& symbol : symbols) {
        writer.String(symbol.c_str());
    }
    writer.EndArray();

    writer.EndObject();
    auto body = s.GetString();

    DLOG(INFO) << "Sending request body to /v2/watchlists: " << body;

    httplib::Client client(environment_.getAPIBaseURL());
    client.set_follow_location(false);
    auto resp = client.Post("/v2/watchlists", headers(environment_), body, kJSONContentType);
    if (!resp) {
        return std::make_pair(Status(1, "Call to /v2/watchlists returned an empty response"), watchlist);
    }

    if (resp->status != 200) {
        std::ostringstream ss;
        ss << "Call to /v2/watchlists returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), watchlist);
    }

    DLOG(INFO) << "Response from /v2/watchlists: " << resp->body;

    return std::make_pair(watchlist.fromJSON(resp->body), watchlist);
}

std::pair<Status, Watchlist> Client::updateWatchlist(const std::string& id,
                                                     const std::string& name,
                                                     const std::vector<std::string>& symbols) const {
    Watchlist watchlist;

    rapidjson::StringBuffer s;
    s.Clear();
    rapidjson::Writer<rapidjson::StringBuffer> writer(s);
    writer.StartObject();

    writer.Key("name");
    writer.String(name.c_str());

    writer.Key("symbols");
    writer.StartArray();
    for (const auto& symbol : symbols) {
        writer.String(symbol.c_str());
    }
    writer.EndArray();

    writer.EndObject();
    auto body = s.GetString();

    auto url = "/v2/watchlists/" + id;
    httplib::Client client(environment_.getAPIBaseURL());
    client.set_follow_location(false);
    DLOG(INFO) << "Sending request to " << url << ": " << body;
    auto resp = client.Put(url.c_str(), headers(environment_), body, kJSONContentType);
    if (!resp) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an empty response";
        return std::make_pair(Status(1, ss.str()), watchlist);
    }

    if (resp->status != 200) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), watchlist);
    }

    DLOG(INFO) << "Response from " << url << ": " << resp->body;
    return std::make_pair(watchlist.fromJSON(resp->body), watchlist);
}

Status Client::deleteWatchlist(const std::string& id) const {
    auto url = "/v2/watchlists/" + id;
    httplib::Client client(environment_.getAPIBaseURL());
    client.set_follow_location(false);
    DLOG(INFO) << "Making request to: " << url;
    auto resp = client.Delete(url.c_str(), headers(environment_));
    if (!resp) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an empty response";
        return Status(1, ss.str());
    }

    if (resp->status != 200 && resp->status != 204) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an HTTP " << resp->status << ": " << resp->body;
        return Status(1, ss.str());
    }
    return Status();
}

std::pair<Status, Watchlist> Client::addSymbolToWatchlist(const std::string& id, const std::string& symbol) const {
    Watchlist watchlist;

    rapidjson::StringBuffer s;
    s.Clear();
    rapidjson::Writer<rapidjson::StringBuffer> writer(s);

    writer.StartObject();
    writer.Key("symbol");
    writer.String(symbol.c_str());
    writer.EndObject();
    auto body = s.GetString();

    auto url = "/v2/watchlists/" + id;
    httplib::Client client(environment_.getAPIBaseURL());
    client.set_follow_location(false);
    DLOG(INFO) << "Making request to: " << url;
    auto resp = client.Post(url.c_str(), headers(environment_), body, kJSONContentType);
    if (!resp) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an empty response";
        return std::make_pair(Status(1, ss.str()), watchlist);
    }

    if (resp->status != 200) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), watchlist);
    }

    DLOG(INFO) << "Response from " << url << ": " << resp->body;

    return std::make_pair(watchlist.fromJSON(resp->body), watchlist);
}

std::pair<Status, Watchlist> Client::removeSymbolFromWatchlist(const std::string& id, const std::string& symbol) const {
    Watchlist watchlist;

    auto url = "/v2/watchlists/" + id + "/" + symbol;
    httplib::Client client(environment_.getAPIBaseURL());
    client.set_follow_location(false);
    DLOG(INFO) << "Making request to: " << url;
    auto resp = client.Delete(url.c_str(), headers(environment_));
    if (!resp) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an empty response";
        return std::make_pair(Status(1, ss.str()), watchlist);
    }

    if (resp->status != 200) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), watchlist);
    }
    return std::make_pair(watchlist.fromJSON(resp->body), watchlist);
}

std::pair<Status, PortfolioHistory> Client::getPortfolioHistory(const std::string& period,
                                                                const std::string& timeframe,
                                                                const std::string& date_end,
                                                                const bool extended_hours) const {
    PortfolioHistory portfolio_history;

    std::string query_string = "";

    if (period != "") {
        if (query_string != "") {
            query_string += "&";
        }
        query_string += "period=" + period;
    }

    if (timeframe != "") {
        if (query_string != "") {
            query_string += "&";
        }
        query_string += "timeframe=" + timeframe;
    }

    if (date_end != "") {
        if (query_string != "") {
            query_string += "&";
        }
        query_string += "date_end=" + date_end;
    }

    if (extended_hours) {
        if (query_string != "") {
            query_string += "&";
        }
        query_string += "extended_hours=true";
    }

    if (query_string != "") {
        query_string = "?" + query_string;
    }

    auto url = "/v2/account/portfolio/history" + query_string;
    DLOG(INFO) << "Making request to: " << url;
    httplib::Client client(environment_.getAPIBaseURL());
    client.set_follow_location(false);
    DLOG(INFO) << "Sending GET request to " << url;
    DLOG(INFO) << "Headers:";
    for (const auto& h : headers(environment_)) {
        DLOG(INFO) << h.first << ": " << h.second;
    }
    auto resp = client.Get(url.c_str(), headers(environment_));
    if (!resp) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an empty response";
        return std::make_pair(Status(1, ss.str()), portfolio_history);
    }

    if (resp->status != 200) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), portfolio_history);
    }

    DLOG(INFO) << "Response from " << url << ": " << resp->body;

    return std::make_pair(portfolio_history.fromJSON(resp->body), portfolio_history);
}

std::pair<Status, Bars> Client::getBars(const std::vector<std::string>& symbols, const std::string& start, const std::string& end, const std::string& after, const std::string& until, const std::string& timeframe, const uint limit) const {
    Bars bars;

    std::string symbols_string = "";

    for (auto i = 0; i < symbols.size(); ++i) {
        symbols_string += symbols[i];
        symbols_string += ",";
    }
    symbols_string.pop_back();

    httplib::Params params{
                           {"symbols", symbols_string},
                           {"limit", std::to_string(limit)},
                           {"timeframe", timeframe},
                           };
    if (start != "") {
        params.insert({"start", start});
    }
    if (end != "") {
        params.insert({"end", end});
    }
    if (after != "") {
        params.insert({"after", after});
    }
    if (until != "") {
        params.insert({"until", until});
    }
    auto query_string = httplib::detail::params_to_query_str(params);

    auto url = "/v2/bars/" + timeframe + "?" + query_string;

    httplib::Client client(environment_.getAPIDataURL());
    client.set_follow_location(false);
    DLOG(INFO) << "Making request to: " << url;
    DLOG(INFO) << "Sending GET request to " << url;
    DLOG(INFO) << "Headers:";
    for (const auto& h : headers(environment_)) {
        DLOG(INFO) << h.first << ": " << h.second;
    }
    auto resp = client.Get(url.c_str(), headers(environment_));
    if (!resp) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an empty response";
        return std::make_pair(Status(1, ss.str()), bars);
    }

    if (resp->status != 200) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), bars);
    }

    DLOG(INFO) << "Response from " << url << ": " << resp->body;
    return std::make_pair(bars.fromJSON(resp->body), bars);
}

std::pair<Status, LastTrade> Client::getLastTrade(const std::string& symbol) const {
    LastTrade last_trade;

    auto url = "/v2/last/stocks/" + symbol;

    httplib::Client client(environment_.getAPIDataURL());
    client.set_follow_location(false);
    DLOG(INFO) << "Making request to: " << url;
    DLOG(INFO) << "Sending GET request to " << url;
    DLOG(INFO) << "Headers:";
    for (const auto& h : headers(environment_)) {
        DLOG(INFO) << h.first << ": " << h.second;
    }
    auto resp = client.Get(url.c_str(), headers(environment_));
    if (!resp) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an empty response";
        return std::make_pair(Status(1, ss.str()), last_trade);
    }

    if (resp->status != 200) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), last_trade);
    }

    DLOG(INFO) << "Response from " << url << ": " << resp->body;
    return std::make_pair(last_trade.fromJSON(resp->body), last_trade);
}

std::pair<Status, LastQuote> Client::getLastQuote(const std::string& symbol) const {
    LastQuote last_quote;

    auto url = "/v2/last_quote/stocks/" + symbol;

    httplib::Client client(environment_.getAPIDataURL());
    client.set_follow_location(false);
    DLOG(INFO) << "Making request to: " << url;
    DLOG(INFO) << "Sending GET request to " << url;
    DLOG(INFO) << "Headers:";
    for (const auto& h : headers(environment_)) {
        DLOG(INFO) << h.first << ": " << h.second;
    }
    auto resp = client.Get(url.c_str(), headers(environment_));
    if (!resp) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an empty response";
        return std::make_pair(Status(1, ss.str()), last_quote);
    }

    if (resp->status != 200) {
        std::ostringstream ss;
        ss << "Call to " << url << " returned an HTTP " << resp->status << ": " << resp->body;
        return std::make_pair(Status(1, ss.str()), last_quote);
    }

    DLOG(INFO) << "Response from " << url << ": " << resp->body;
    return std::make_pair(last_quote.fromJSON(resp->body), last_quote);
}

void Client::logResponseDetails(const std::unique_ptr<httplib::Response>& response) const {
    if (response) {
        DLOG(INFO) << "Response Status: " << response->status;
        DLOG(INFO) << "Response Headers:";
        for (const auto& header : response->headers) {
            DLOG(INFO) << header.first << ": " << header.second;
        }
        DLOG(INFO) << "Response Body: " << response->body;
    } else {
        DLOG(INFO) << "Response is empty or null.";
    }
}

std::pair<Status, std::unordered_map<std::string, Trade>> Client::getLatestTrades(const std::vector<std::string>& symbols) const {
    std::unordered_map<std::string, Trade> trades;

    if (symbols.empty()) {
        return {Status(true, "No symbols provided"), trades};
    }

    // Construct the comma-separated symbols parameter
    std::string symbol_param = symbols[0];

    for (size_t i = 1; i < symbols.size(); ++i) {
        symbol_param += "," + symbols[i];
    }

    std::string endpoint = "/v2/stocks/snapshots?symbols=" + symbol_param;

    // Initialize HTTP client
    httplib::Client client(environment_.getAPIDataURL());
    client.set_default_headers(headers(environment_));

    // Perform GET request to retrieve snapshot data
    auto res = client.Get(endpoint.c_str());

    if (!res) {
        return {Status(false, "HTTP request failed"), trades};
    }
    if (res->status != 200) {
        return {Status(false, "HTTP error: " + std::to_string(res->status)), trades};
    }

    // Parse the JSON response
    rapidjson::Document doc;
    doc.Parse(res->body.c_str());

    if (doc.HasParseError()) {
        return {Status(false, "Failed to parse JSON response"), trades};
    }

    // Process each symbol's snapshot data
    for (const auto& symbol : symbols) {
        if (doc.HasMember(symbol.c_str()) && doc[symbol.c_str()].IsObject()) {
            const auto& snapshot = doc[symbol.c_str()];
            if (snapshot.HasMember("latestTrade") && snapshot["latestTrade"].IsObject()) {
                const auto& trade_info = snapshot["latestTrade"];

                // Extract trade details
                Trade trade;
                if (trade_info.HasMember("p") && trade_info["p"].IsDouble()) {
                    trade.price = trade_info["p"].GetDouble();
                }
                if (trade_info.HasMember("t") && trade_info["t"].IsString()) {
                    trade.timestamp = parseTimestamp(trade_info["t"].GetString());
                }
                trades[symbol] = trade;
            }
        }
    }

    return {Status(), trades};
}

uint64_t Client::parseTimestamp(const std::string& timestamp) const {
    std::tm tm = {};
    std::istringstream ss(timestamp);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");  // Adjust format based on timestamp precision
    return static_cast<uint64_t>(std::mktime(&tm));
}
} // namespace alpaca
