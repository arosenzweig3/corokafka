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
#ifndef BLOOMBERG_COROKAFKA_DESERIALIZER_H
#define BLOOMBERG_COROKAFKA_DESERIALIZER_H

#include <corokafka/corokafka_message.h>
#include <corokafka/corokafka_utils.h>
#include <corokafka/corokafka_header_pack.h>
#include <boost/any.hpp>

namespace Bloomberg {
namespace corokafka {

struct DeserializerError
{
    enum class Source : uint8_t {
        Kafka           = 1<<0,
        Key             = 1<<1,
        Payload         = 1<<2,
        Header          = 1<<3,
        Preprocessor    = 1<<4
    };
    bool isKafkaError() const { return (_source & (uint8_t)Source::Kafka) != 0; }
    bool isKeyError() const { return (_source & (uint8_t)Source::Key) != 0; }
    bool isPayloadError() const { return (_source & (uint8_t)Source::Payload) != 0; }
    bool isHeaderError() const { return (_source & (uint8_t)Source::Header) != 0; }
    bool isPreprocessorError() const { return (_source & (uint8_t)Source::Preprocessor) != 0; }
    
    //members
    Error       _error{RD_KAFKA_RESP_ERR_NO_ERROR};
    uint8_t     _source{0};
    int         _headerNum{-1};
};

struct Deserializer
{
    using ResultType = boost::any;
    using result_type = ResultType; //cppkafka compatibility for callback invoker
    virtual ~Deserializer() = default;
    virtual ResultType operator()(const TopicPartition&, const Buffer&) const { return {}; };
    virtual ResultType operator()(const TopicPartition&, const HeaderPack&, const Buffer&) const { return {}; }
    virtual explicit operator bool() const = 0;
};

template <typename T>
class ConcreteDeserializer : public Deserializer
{
public:
    using ResultType = Deserializer::ResultType;
    using Callback = std::function<T(const TopicPartition&, const Buffer& buffer)>;
    
    //Ctor
    ConcreteDeserializer(Callback callback) :
        _func(std::move(callback))
    {}
    
    const Callback& getCallback() const { return _func; }
    
    ResultType operator()(const TopicPartition& toppar,
                          const Buffer& buffer) const final {
        return _func(toppar, buffer);
    }
    
    explicit operator bool() const final { return (bool)_func; }
private:
    Callback _func;
};

template <typename T>
class ConcreteDeserializerWithHeaders : public Deserializer
{
public:
    using ResultType = Deserializer::ResultType;
    using Callback = std::function<T(const TopicPartition&, const HeaderPack&, const Buffer&)>;
    
    //Ctor
    ConcreteDeserializerWithHeaders(Callback callback) :
        _func(std::move(callback))
    {}
    
    const Callback& getCallback() const { return _func; }
    
    ResultType operator()(const TopicPartition& toppar,
                          const HeaderPack& headers,
                          const Buffer& buffer) const final {
        return _func(toppar, headers, buffer);
    }
    
    explicit operator bool() const final { return (bool)_func; }
private:
    Callback _func;
};

}
}

#endif //BLOOMBERG_COROKAFKA_DESERIALIZER_H
