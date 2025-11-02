#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "Protocols/HTTP/RouterCommon.h"

namespace usub::server::protocols::http {


    struct ParamEdge;

    struct param_constraint {
        std::string pattern;
        std::string description;
    };
    const param_constraint default_constraint{
            R"([^/]+)",
            "Encountered an error..."};
    const std::unordered_map<std::string_view, const param_constraint *> no_constraints{};

    struct RadixNode {
        std::unordered_map<std::string, std::unique_ptr<RadixNode>> literal;// литеральные дети
        std::vector<ParamEdge> param;                                       // регексовые
        std::unique_ptr<RadixNode> wildcard;                                // *
        std::string wildcard_name;                                          // имя
        bool trailing_slash = false;
        std::unique_ptr<Route> route = nullptr;
    };

    struct ParamEdge {
        std::string name;               // имя параметра (id:)
        std::optional<std::regex> regex;// пусто -> [^/]+
        std::unique_ptr<RadixNode> child;
        std::optional<param_constraint> constraint;
    };

    class RadixRouter {
    public:
        RadixRouter() : root_(std::make_unique<RadixNode>()) {}

        Route &addRoute(const std::set<std::string> &methods,
                        const std::string &pattern,
                        std::function<FunctionType> handler,
                        const std::unordered_map<std::string_view, const param_constraint *> &constraints = no_constraints);

        Route &addPlainStringHandler(const std::set<std::string> &method, const std::string &pathPattern, std::function<FunctionType> function);

        Route &addHandler(const std::set<std::string> &method, const std::string &pathPattern, std::function<FunctionType> function);

        Route &addHandler(std::string_view method, const std::string &pathPattern, std::function<FunctionType> function);

        Route &addHandler(const std::set<std::string> &method,
                          const std::string &pathPattern,
                          std::function<FunctionType> function,
                          std::unordered_map<std::string_view, const param_constraint *> &&constraints = {});

        std::optional<std::pair<Route *, bool>> match(Request &request, std::string *error_description = nullptr);

        MiddlewareChain &addMiddleware(MiddlewarePhase phase, std::function<MiddlewareFunctionType> middleware);

        MiddlewareChain &getMiddlewareChain();

        void addErrorHandler(const std::string &error_code, std::function<FunctionType> function);

        void executeErrorChain(Request &request, Response &response);

        std::string dump() const;

    private:
        std::unique_ptr<RadixNode> root_;
        std::vector<Route> routes_;
        std::unordered_map<std::string, std::function<FunctionType>> error_page_handlers_;
        MiddlewareChain middleware_chain_;

        struct Segment {
            enum Kind {
                Lit,
                Par,
                Wild
            } kind;
            std::string lit, name, re;
            std::optional<param_constraint> constraint;
        };

        void parsePathPattern(const std::string &pathPattern,
                              std::regex &outRegex,
                              std::vector<std::string> &outParamNames,
                              const std::unordered_map<std::string_view, const param_constraint *> &constraints = {}) const;

        size_t findMatchingBrace(const std::string &pathPattern, size_t start) const;

        bool containsCapturingGroup(const std::string &regex) const;

        std::vector<std::string> splitPath(const std::string &path);

        std::vector<Segment> parseSegments(const std::string &pattern, std::vector<std::string> &param_names) const;

        void applyConstraints(std::vector<Segment> &segs, const std::unordered_map<std::string_view, const param_constraint *> &constraints);

        void insert(RadixNode *node, const std::vector<Segment> &segs, std::size_t idx, std::unique_ptr<Route> &route, bool has_trailing_slash);

        bool matchDFS(RadixNode* node,
                      const std::vector<std::string>& segs,
                      std::size_t idx,
                      Request& req,
                      Route*& out,
                      std::string* last_error);

        bool matchIter(RadixNode *node,
                       const std::vector<std::string> &segs,
                       Request &req,
                       Route *&out,
                       std::string *last_error = nullptr);

        void printNode(const RadixNode *node,
                       std::ostringstream &buf,
                       const std::string &prefix) const;
    };

}// namespace usub::server::protocols::http
