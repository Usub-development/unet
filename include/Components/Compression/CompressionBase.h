#ifndef COMPRESSION_BASE_H
#define COMPRESSION_BASE_H

#include <functional>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "utils/TypeID.h"

#ifdef ERROR
#undef ERROR
#endif

namespace usub::server::component {

    // Base class for all decoders
    class CompressionBase {
    public:
        enum STATE {
            MORE_DATA_NEEDED = 0,
            PARTIAL = 1,
            COMPLETE = 2,
            ERROR = 3
        };

    protected:
        mutable STATE state_{STATE::MORE_DATA_NEEDED};

    private:
        const std::string stored_data_type_{};

    public:
        CompressionBase(std::string data_type) : stored_data_type_(data_type) {};
        virtual ~CompressionBase() = default;
        virtual void decompress(std::string &data) const = 0;// Decompress a string in place
        virtual void compress(std::string &data) const = 0;  // Compress a string in place

        virtual std::string decompress(const std::string &data) const {
            std::string rv = data;
            this->decompress(rv);
            return rv;
        }

        virtual std::string compress(const std::string &data) const {
            std::string rv = data;
            this->compress(rv);
            return rv;
        }

        /**
         * @brief Decompress a stream of data
         * 
         * @param data - the data to decompress
         * 
         * @details This function is used to decompress a stream of data, prefered way is by obtaining ownership of the data
         * by moving it to the function.
         */
        // virtual void stream_decompress(std::string &data) const = 0;
        // virtual void stream_compress(std::string &data) const = 0;

        // virtual std::string stream_decompress(const std::string &data) const {
        //     std::string rv = data;
        //     this->stream_decompress(rv);
        //     return rv;
        // }

        // virtual std::string stream_compress(const std::string &data) const {
        //     std::string rv = data;
        //     this->stream_compress(rv);
        //     return rv;
        // }

        /**
         * @brief Get the State of current object
         * 
         * @return uint8_t the state of the parser
         * 
         * 00000000 (0) = Waiting for more data
         * 00000001 (1) = Partial Data
         * 00000011 (2) = Complete data
         * 00000100 (3) = Error will result in 400 status code
         * otheres and/or combinations are unused for now
         */
        virtual uint8_t getState() const = 0;

        virtual size_t getTypeID() const = 0;

        const std::string getTypeName() const {
            return this->stored_data_type_;
        }
    };

}// namespace usub::server::component

// Singleton DecoderChain Factory
class DecoderChainFactory {
public:
    using CreatorFunc = std::function<std::unique_ptr<usub::server::component::CompressionBase>()>;

    // Singleton access
    static DecoderChainFactory &instance() {
        static DecoderChainFactory factoryInstance;
        return factoryInstance;
    }

    // Register a decoder with its identifier
    bool registerDecoder(const std::string &encoding, CreatorFunc creator) {
        auto result = registry_.emplace(encoding, std::move(creator));
        return result.second;
    }

    // Create a decoder chain based on predefined encodings
    std::vector<std::unique_ptr<usub::server::component::CompressionBase>> create(const std::vector<std::string> &encodings) const {
        std::vector<std::unique_ptr<usub::server::component::CompressionBase>> chain;

        for (auto encoding_iter = encodings.rbegin(); encoding_iter != encodings.rend(); ++encoding_iter) {
            if (*encoding_iter == "chunked") {
                continue;
            }

            auto registry_iter = registry_.find(*encoding_iter);
            if (registry_iter != registry_.end()) {
                chain.push_back(registry_iter->second());
            } else {
                throw std::runtime_error("Unsupported encoding: " + *encoding_iter);
            }
        }

        return chain;
    }

    void create(const std::vector<std::string> &encodings,
                std::vector<std::shared_ptr<usub::server::component::CompressionBase>> &chain) {
        for (auto encoding_iter = encodings.rbegin(); encoding_iter != encodings.rend(); ++encoding_iter) {
            auto registry_iter = registry_.find(*encoding_iter);
            if (registry_iter != registry_.end()) {
                chain.push_back(registry_iter->second());
            } else {
                throw std::runtime_error("Unsupported encoding: " + *encoding_iter);
            }
        }
    }

private:
    DecoderChainFactory() = default;
    DecoderChainFactory(const DecoderChainFactory &) = delete;
    DecoderChainFactory &operator=(const DecoderChainFactory &) = delete;

    std::unordered_map<std::string, CreatorFunc> registry_;
};

// Registration helper
class DecoderRegistrar {
public:
    template<typename DecoderClass>
    static bool registerDecoder(const std::string &encoding) {
        return DecoderChainFactory::instance().registerDecoder(
                encoding,
                []() -> std::unique_ptr<usub::server::component::CompressionBase> {
                    return std::make_unique<DecoderClass>();
                });
    }
};

#endif// COMPRESSION_BASE_H