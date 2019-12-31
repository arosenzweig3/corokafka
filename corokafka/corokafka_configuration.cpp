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
#include <corokafka/corokafka_configuration.h>
#include <corokafka/corokafka_exception.h>

namespace Bloomberg {
namespace corokafka {

//========================================================================
//                             CONFIGURATION
//========================================================================
const std::string& Configuration::getJsonSchema()
{
    static const std::string jsonSchema = R"JSON(
    {
        "$schema" : "http://json-schema.org/draft-04/schema#",
        "$id" : "bloomberg:corokafka.json",
        "definitions": {
            "connector": {
                "title": "CoroKafka configuration",
                "type": "object",
                "properties": {
                    "pollIntervalMs": {
                        "type":"number",
                        "default":100
                    },
                    "maxMessagePayloadOutputLength": {
                        "type":"number",
                        "default":100
                    },
                    "quantum": {
                        "$ref": "bloomberg:quantum.json"
                    },
                },
                "additionalProperties": false,
                "required": []
            },
            "option": {
                "title": "Internal options for corokafka, cppkafka and rdkafka",
                "type": "object",
                "patternProperties": {
                    "^.*$": {
                        "anyOf": [
                            {"type":"number"},
                            {"type":"boolean"},
                            {"type":"string"}
                        ],
                        "examples": ["metadata.broker.list", "internal.producer.payload.policy"]
                    }
                }
            },
            "partition": {
                "title": "A kafka partition",
                "type": "object",
                "properties": {
                    "ids": {
                        "description" : "Partition id(s). Empty = all partitions, one value = single partition, two values = range [first, second]",
                        "type":"array",
                        "items": { "type": "number" },
                        "minItems": 0,
                        "maxItems": 2,
                        "uniqueItems": true
                    },
                    "offset": {
                        "description": "A partition offset. Values are: -1000(stored),-1(begin),-2(end),>=0(exact or relative)",
                        "type":"number",
                        "default":-1000
                    },
                    "relative": {
                        "description": "If true, the offset represents the Nth message before the stored offset (i.e. stored-N).",
                        "type":"boolean",
                        "default": false
                    }
                },
                "additionalProperties": false,
                "required": []
            },
            "partitionConfig": {
                "title": "Partition assignment configuration for a topic.",
                "type": "object",
                "properties": {
                    "strategy": {
                        "description":"Only applies to consumer topic configurations",
                        "type":"string",
                        "enum":["static","dynamic"],
                        "default":"dynamic"
                    },
                    "partitions": {
                        "description":"Only applies to consumer topic configurations",
                        "type":"array",
                        "items": { "$ref" : "#/definitions/partition" }
                    }
                },
                "additionalProperties": false,
                "required": []
            },
            "topicConfig": {
                "title": "Consumer or producer topic configuration",
                "type": "object",
                "properties": {
                    "name": {
                        "description": "The name of this configuration object",
                        "type":"string"
                    },
                    "type": {
                        "type":"string",
                        "enum": ["producer", "consumer"]
                    },
                    "options": {
                        "description": "The rdkafka and corokafka options for this consumer/producer. Must at least contain 'metadata.broker.list'",
                        "$ref" : "#/definitions/option"
                    },
                    "topicOptions": {
                        "description": "The rdkafka and corokafka topic options for this consumer/producer",
                        "$ref" : "#/definitions/option"
                    }
                },
                "additionalProperties": false,
                "required": ["name","type"]
            },
            "topic": {
                "title": "Consumer or producer topic",
                "type": "object",
                "properties": {
                    "name": {
                        "description": "The name of this topic",
                        "type":"string"
                    },
                    "config": {
                        "description": "The config for this topic",
                        "type":"string"
                    },
                    "assignment": {
                        "description": "The partition strategy and assignment (consumers only)",
                        "$ref" : "#/definitions/partitionConfig"
                    }
                },
                "additionalProperties": false,
                "required": ["name","config"]
            }
        },

        "title": "Kafka connector settings",
        "type": "object",
        "properties": {
            "connector": { "$ref":"#/definitions/connector" },
            "topicConfigs": {
                "type":"array",
                "items": { "$ref": "#/definitions/topicConfig" },
                "minItems": 1,
                "uniqueItems": true
            },
            "topics": {
                "type":"array",
                "items": { "$ref": "#/definitions/topic" },
                "minItems": 1,
                "uniqueItems": false
            }
        },
        "additionalProperties": false,
        "required": [ "topics","topicConfigs" ]
    }
    )JSON";
    return jsonSchema;
}

const std::string& Configuration::getJsonSchemaUri()
{
    static std::string uri = "bloomberg:corokafka.json";
    return uri;
}

Configuration::Configuration(OptionList options)
{
    _options[(int)OptionType::All] = std::move(options);
}

Configuration::Configuration(std::initializer_list<cppkafka::ConfigurationOption> options)
{
    _options[(int)OptionType::All] = std::move(options);
}

const Configuration::OptionList& Configuration::getOptions(OptionType type) const
{
    return _options[(int)type];
}

const cppkafka::ConfigurationOption* Configuration::getOption(const std::string& name) const
{
    return findOption(name, _options[(int)OptionType::All]);
}

const cppkafka::ConfigurationOption* Configuration::findOption(const std::string& name,
                                                               const OptionList& config)
{
    const auto it = std::find_if(config.cbegin(), config.cend(),
                                 [&name](const cppkafka::ConfigurationOption& config)->bool {
        return StringEqualCompare()(config.get_key(), name);
    });
    if (it != config.cend()) {
        return &*it;
    }
    return nullptr;
}

void Configuration::parseOptions(const std::string& optionsPrefix,
                                 const OptionSet& allowed,
                                 OptionList(&optionList)[3])
{
    OptionList& internal = optionList[(int)OptionType::Internal];
    OptionList& rdKafka = optionList[(int)OptionType::RdKafka];
    const OptionList& config = optionList[(int)OptionType::All];
    
    if (!allowed.empty()) {
        for (const auto& option : config) {
            if (StringEqualCompare()(option.get_key(), optionsPrefix, optionsPrefix.length())) {
                auto it = allowed.find(option.get_key());
                if (it == allowed.end()) {
                    throw InvalidOptionException(option.get_key(), "Invalid");
                }
                //this is an internal option
                internal.emplace_back(option);
            }
            else {
                //rdkafka option
                rdKafka.emplace_back(option);
            }
        }
    }
    else {
        rdKafka.assign(std::make_move_iterator(config.begin()),
                       std::make_move_iterator(config.end()));
    }
}

}
}
