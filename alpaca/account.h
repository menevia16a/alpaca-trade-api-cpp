#pragma once

#include "status.h"

#include <string>

namespace alpaca {
    /**
    * @brief A type representing an Alpaca account.
    */
    class Account {
    public:
        /**
        * @brief A method for deserializing JSON into the current object state.
        *
        * @param json The JSON string
        *
        * @return a Status indicating the success or faliure of the operation.
        */
        Status fromJSON(const std::string& json);

        bool account_blocked;
        bool pattern_day_trader;
        bool shorting_enabled;
        bool trade_suspended_by_user;
        bool trading_blocked;
        bool transfers_blocked;

        int daytrade_count;

        std::string account_number;
        std::string buying_power;
        std::string cash;
        std::string created_at;
        std::string currency;
        std::string daytrading_buying_power;
        std::string equity;
        std::string id;
        std::string initial_margin;
        std::string last_equity;
        std::string last_maintenance_margin;
        std::string long_market_value;
        std::string maintenance_margin;
        std::string multiplier;
        std::string portfolio_value;
        std::string regt_buying_power;
        std::string short_market_value;
        std::string sma;
        std::string status;
    };

    /**
    * @brief A type representing Alpaca account configuration.
    */
    class AccountConfigurations {
    public:
        /**
        * @brief A method for deserializing JSON into the current object state.
        *
        * @param json The JSON string
        *
        * @return a Status indicating the success or faliure of the operation.
        */
        Status fromJSON(const std::string& json);

        bool no_shorting;
        bool suspend_trade;

        std::string dtbp_check;
        std::string trade_confirm_email;
    };

    /**
    * @brief A type representing trade activity from an Alpaca account.
    */
    class TradeActivity {
    public:
        /**
        * @brief A method for deserializing JSON into the current object state.
        *
        * @param json The JSON string
        *
        * @return a Status indicating the success or faliure of the operation.
        */
        Status fromJSON(const std::string& json);

        std::string activity_type;
        std::string cum_qty;
        std::string id;
        std::string leaves_qty;
        std::string order_id;
        std::string price;
        std::string qty;
        std::string side;
        std::string symbol;
        std::string transaction_time;
        std::string type;
    };

    /**
    * @brief A type representing non-trade activity from an Alpaca account.
    */
    class NonTradeActivity {
    public:
        /**
        * @brief A method for deserializing JSON into the current object state.
        *
        * @param json The JSON string
        *
        * @return a Status indicating the success or faliure of the operation.
        */
        Status fromJSON(const std::string& json);

        std::string activity_type;
        std::string date;
        std::string id;
        std::string net_amount;
        std::string per_share_amount;
        std::string qty;
        std::string symbol;
    };
} // namespace alpaca
