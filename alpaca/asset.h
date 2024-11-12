#pragma once

#include "status.h"

#include <string>

namespace alpaca {
    /**
    * @brief The class of an asset.
    */

    enum AssetClass {
        USEquity,
    };

    /**
    * @brief A helper to convert an AssetClass to a string
    */
    std::string assetClassToString(const AssetClass asset_class);

    /**
    * @brief A type representing an Alpaca asset.
    */
    class Asset {
    public:
        /**
        * @brief A method for deserializing JSON into the current object state.
        *
        * @param json The JSON string
        *
        * @return a Status indicating the success or faliure of the operation.
        */
        Status fromJSON(const std::string& json);

        bool easy_to_borrow;
        bool marginable;
        bool shortable;
        bool tradable;

        std::string id;
        std::string name;
        std::string symbol;
        std::string asset_class;
        std::string exchange;
        std::string status;
    };
} // namespace alpaca
