#pragma once

#include "status.h"

#include <string>

namespace alpaca {
    /**
    * @brief A type representing a calendar day.
    */
    class Date {
    public:
        /**
        * @brief A method for deserializing JSON into the current object state.
        *
        * @param json The JSON string
        *
        * @return a Status indicating the success or faliure of the operation.
        */
        Status fromJSON(const std::string& json);

        std::string close;
        std::string date;
        std::string open;
    };
} // namespace alpaca
