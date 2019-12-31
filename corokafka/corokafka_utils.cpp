/*
** Copyright 2019 Bloomberg Finance L.P.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
#include <corokafka/corokafka_utils.h>
#include <corokafka/corokafka_configuration.h>
#include <corokafka/corokafka_producer_metadata.h>
#include <corokafka/corokafka_consumer_metadata.h>
#include <corokafka/corokafka_producer_topic_entry.h>
#include <corokafka/corokafka_consumer_topic_entry.h>
#include <corokafka/corokafka_exception.h>

namespace Bloomberg {
namespace corokafka {

ssize_t& maxMessageBuilderOutputLength()
{
    static ssize_t messageLen{100};
    return messageLen;
}

cppkafka::LogLevel logLevelFromString(const std::string& level)
{
    StringEqualCompare compare;
    if (compare(level, "emergency")) {
        return cppkafka::LogLevel::LogEmerg;
    }
    if (compare(level, "alert")) {
        return cppkafka::LogLevel::LogAlert;
    }
    if (compare(level, "critical")) {
        return cppkafka::LogLevel::LogCrit;
    }
    if (compare(level, "error")) {
        return cppkafka::LogLevel::LogErr;
    }
    if (compare(level, "warning")) {
        return cppkafka::LogLevel::LogWarning;
    }
    if (compare(level, "notice")) {
        return cppkafka::LogLevel::LogNotice;
    }
    if (compare(level, "info")) {
        return cppkafka::LogLevel::LogInfo;
    }
    if (compare(level, "debug")) {
        return cppkafka::LogLevel::LogDebug;
    }
    throw InvalidArgumentException(0, "Unknown log level");
}

void handleException(const std::exception& ex,
                     const Metadata& metadata,
                     const TopicConfiguration& config,
                     cppkafka::LogLevel level)
{
    cppkafka::CallbackInvoker<Callbacks::ErrorCallback> error_cb("error", config.getErrorCallback(), nullptr);
    const cppkafka::HandleException* hex = dynamic_cast<const cppkafka::HandleException*>(&ex);
    if (error_cb) {
        if (hex) {
            error_cb(metadata, hex->get_error(), hex->what(), nullptr);
        }
        else {
            error_cb(metadata, RD_KAFKA_RESP_ERR_UNKNOWN, ex.what(), nullptr);
        }
    }
    if (level >= cppkafka::LogLevel::LogErr) {
        cppkafka::CallbackInvoker<Callbacks::LogCallback> logger_cb("log", config.getLogCallback(), nullptr);
        if (logger_cb) {
            logger_cb(metadata, cppkafka::LogLevel::LogErr, "corokafka", ex.what());
        }
    }
}

}
}

