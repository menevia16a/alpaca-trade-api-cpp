#pragma once

#include "status.h"
#include "rapidjson/document.h"

#include <map>
#include <string>
#include <vector>
#include <cstdint>

namespace alpaca {
    /**
    * @brief A type representing an Alpaca bar.
    */
    class Bar {
    public:
        /**
        * @brief A method for deserializing JSON into the current object state.
        *
        * @param json The JSON string
        *
        * @return a Status indicating the success or faliure of the operation.
        */
        Status fromJSON(const std::string& json);

        /**
        * @brief A method for deserializing JSON into the current object state.
        *
        * @param json The JSON string
        *
        * @return a Status indicating the success or faliure of the operation.
        */
        Status fromDocument(const rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>>& d);

    public:
        uint32_t time;
        uint32_t volume;

        double open_price;
        double high_price;
        double low_price;
        double close_price;
    };

    /**
    * @brief A type representing bars for multiple symbols
    */
    class Bars {
    public:
        /**
        * @brief A method for deserializing JSON into the current object state.
        *
        * @param json The JSON string
        *
        * @return a Status indicating the success or faliure of the operation.
        */
        Status fromJSON(const std::string& json);

        public:
        std::map<std::string, std::vector<Bar>> bars;
    };
} // namespace alpaca
