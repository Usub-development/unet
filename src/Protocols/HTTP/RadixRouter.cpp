#include "Protocols/HTTP/RadixRouter.h"


namespace usub::server::protocols::http {

    size_t RadixRouter::findMatchingBrace(const std::string &pathPattern, size_t start) const {
        std::stack<char> braceStack;
        for (size_t i = start; i < pathPattern.size(); ++i) {
            if (pathPattern[i] == '{') {
                braceStack.push('{');
            } else if (pathPattern[i] == '}') {
                if (braceStack.empty()) {
                    throw std::runtime_error("Unmatched '}' in path pattern");
                }
                braceStack.pop();
                if (braceStack.empty()) {
                    return i;
                }
            }
        }
        return std::string::npos;
    }

    bool RadixRouter::containsCapturingGroup(const std::string &rx) const {
        for (size_t i = 0; i < rx.size(); ++i) {
            if (rx[i] == '(') {
                if (i + 1 >= rx.size() || rx[i + 1] != '?') {
                    return true;// захватывающая группа
                }
            }
        }
        return false;
    }

    std::vector<std::string> RadixRouter::splitPath(const std::string &path) {
        std::vector<std::string> parts;
        parts.reserve(8);
        std::string token;
        std::stringstream ss(path);

        if (!path.empty() && path[0] == '/') {
            ss.get();// skip first '/'
        }

        while (std::getline(ss, token, '/')) {
            if (!token.empty()) parts.push_back(token);
        }

        return parts;
    }

    std::vector<RadixRouter::Segment>
    RadixRouter::parseSegments(const std::string &pattern, std::vector<std::string> &param_names) const {
        std::vector<RadixRouter::Segment> segs;
        std::string token;
        bool escape = false;
        std::stringstream ss(pattern);

        if (!pattern.empty() && pattern[0] == '/') {
            ss.get();
        }

        while (std::getline(ss, token, '/')) {
            if (token.empty()) continue;

            std::string actual;
            escape = false;
            for (size_t i = 0; i < token.size(); ++i) {
                if (token[i] == '\\' && !escape) {
                    escape = true;
                    continue;
                }
                actual += token[i];
                escape = false;
            }

            const bool is_param = actual.size() >= 2 && actual.front() == '{' && actual.back() == '}';
            const bool is_wildcard = (actual == "*" && token != "\\*");

            if (!is_param && !is_wildcard) {
                segs.push_back({Segment::Lit, actual, {}, {}, std::nullopt});
                continue;
            }

            if (is_param) {
                std::string body = actual.substr(1, actual.size() - 2);
                std::size_t colon = body.find(':');

                std::string name = body.substr(0, colon);
                std::string rx = (colon == std::string::npos) ? "" : body.substr(colon + 1);

                param_names.push_back(name);
                segs.push_back({Segment::Par, {}, name, rx, std::nullopt});
                continue;
            }

            segs.push_back({Segment::Wild, {}, "*", {}, std::nullopt});
        }
        return segs;
    }

    void RadixRouter::applyConstraints(std::vector<Segment> &segs,
                                       const std::unordered_map<std::string_view, const param_constraint *> &constraints) {
        for (auto &seg: segs) {
            if (seg.kind != Segment::Par) continue;

            const param_constraint *constraint = nullptr;
            auto it = constraints.find(seg.name);
            if (it != constraints.end() && it->second) {
                constraint = it->second;
            }

            if (constraint) {
                seg.re = constraint->pattern;
                seg.constraint = *constraint;
            } else if (seg.re.empty()) {
                seg.re = default_constraint.pattern;
                seg.constraint = default_constraint;
            }
        }
    }

    void RadixRouter::insert(RadixNode *node,
                             const std::vector<Segment> &segs,
                             std::size_t idx,
                             std::unique_ptr<Route> &route,
                             bool has_trailing_slash) {
        if (idx == segs.size()) {
            node->route = std::move(route);
            node->trailing_slash = has_trailing_slash;
            return;
        }

        const Segment &cur = segs[idx];

        if (cur.kind == Segment::Lit) {
            auto &child = node->literal[cur.lit];
            if (!child) child = std::make_unique<RadixNode>();
            insert(child.get(), segs, idx + 1, route, has_trailing_slash);
            return;
        }

        if (cur.kind == Segment::Par) {
            // Сливаем ребро только если совпадают и имя, и паттерн (через constraint)
            for (ParamEdge &edge: node->param) {
                if (edge.constraint && cur.constraint && edge.constraint->pattern == cur.constraint->pattern && edge.name == cur.name) {
                    insert(edge.child.get(), segs, idx + 1, route, has_trailing_slash);
                    return;
                }
            }

            ParamEdge edge;
            edge.name = cur.name;
            edge.regex = std::regex(cur.re);
            edge.child = std::make_unique<RadixNode>();
            edge.constraint = cur.constraint;
            // edge.pattern_str = cur.re;

            node->param.push_back(std::move(edge));                                          // сначала добавляем
            insert(node->param.back().child.get(), segs, idx + 1, route, has_trailing_slash);// потом уходим вниз
            return;
        }

        // Wildcard
        if (!node->wildcard) node->wildcard = std::make_unique<RadixNode>();
        node->wildcard_name = cur.name.empty() ? "*" : cur.name;
        insert(node->wildcard.get(), segs, segs.size(), route, has_trailing_slash);
    }

    Route &RadixRouter::addRoute(const std::set<std::string> &methods,
                                 const std::string &pattern,
                                 std::function<FunctionType> handler,
                                 const std::unordered_map<std::string_view, const param_constraint *> &constraints) {
        std::vector<std::string> param_names;
        std::vector<Segment> segs = parseSegments(pattern, param_names);

        applyConstraints(segs, constraints);
        auto routePtr = std::make_unique<Route>(
                methods, std::regex{}, param_names, std::move(handler),
                methods.contains("*"));

        Route *rawPtr = routePtr.get();
        const bool has_trailing_slash = !pattern.empty() && pattern.back() == '/';
        insert(root_.get(), segs, 0, routePtr, has_trailing_slash);

        std::ostringstream methods_stream;
        for (auto it = methods.begin(); it != methods.end(); ++it) {
            if (it != methods.begin()) methods_stream << ',';
            methods_stream << *it;
        }

        std::ostringstream path_stream;
        path_stream << '/';
        for (size_t i = 0; i < segs.size(); ++i) {
            if (i) path_stream << '/';
            const auto &s = segs[i];
            switch (s.kind) {
                case Segment::Lit:
                    path_stream << s.lit;
                    break;
                case Segment::Par: {
                    path_stream << '{' << s.name << '}';
                    const param_constraint *pc = nullptr;
                    if (auto it = constraints.find(s.name); it != constraints.end() && it->second) {
                        pc = it->second;
                    } else if (s.constraint) {
                        pc = &(*s.constraint);
                    }
                    if (pc) {
                        path_stream << '('
                                    << "constraint_name:" << s.name
                                    << "|constraint_desc:" << pc->description
                                    << "|constraint_regex:" << pc->pattern
                                    << ')';
                    }
                    break;
                }
                case Segment::Wild:
                    path_stream << '*';
                    break;
            }
        }
        if (has_trailing_slash) path_stream << '/';

        std::cout << "route methods: " << methods_stream.str() << "\n"
                  << "path: " << path_stream.str() << "\n"
                  << "hint: router.addHandler({\"" << methods_stream.str()
                  << "\"}, \"" << pattern << "\", handlerFunction);\n"
                  << std::endl;

        return *rawPtr;
    }

    std::optional<std::pair<Route *, bool>>
    RadixRouter::match(Request &request, std::string *error_description) {
        const std::string path = request.getURL();

        std::vector<std::string> segs = splitPath(path);
        Route *routePtr = nullptr;
        if (!matchIter(root_.get(), segs, request, routePtr, error_description) || !routePtr) {
            return std::nullopt;
        }

        bool methodOk = routePtr->accept_all_methods ||
                        routePtr->allowed_method_tokenns.contains(request.getRequestMethod());

        return std::make_pair(routePtr, methodOk);
    }

    MiddlewareChain &RadixRouter::addMiddleware(MiddlewarePhase phase,
                                                std::function<MiddlewareFunctionType> middleware) {
        if (phase == MiddlewarePhase::HEADER) {
            this->middleware_chain_.addMiddleware(phase, std::move(middleware));
        } else {
            std::cerr << "Non header global middlewares are not supported yet" << std::endl;
        }
        return this->middleware_chain_;
    }

    MiddlewareChain &RadixRouter::getMiddlewareChain() {
        return this->middleware_chain_;
    }

    void RadixRouter::addErrorHandler(const std::string &error_code, std::function<FunctionType> function) {
        this->error_page_handlers_.emplace(error_code, function);
    }

    void RadixRouter::executeErrorChain(Request &request, Response &response) {
        if (this->error_page_handlers_.contains("Log")) {
            this->error_page_handlers_.at("Log")(request, response);
        }
        std::string error = std::to_string((uint16_t) request.getState());
        if (this->error_page_handlers_.contains(error)) {
            this->error_page_handlers_.at(error)(request, response);
        }
    }

    void RadixRouter::parsePathPattern(const std::string &pathPattern,
                                       std::regex &outRegex,
                                       std::vector<std::string> &outParamNames,
                                       const std::unordered_map<std::string_view, const param_constraint *> &constraints) const {
        std::string regexPattern = "^";
        size_t position = 0;

        if (containsCapturingGroup(pathPattern)) {
            throw std::runtime_error("Path pattern contains capturing groups, which are not allowed: \"" + pathPattern + "\"");
        }

        auto appendEsc = [&](char ch) {
            static const std::string specials = ".^$+?()[]\\|";
            if (specials.find(ch) != std::string::npos) regexPattern += '\\';
            regexPattern += ch;
        };

        while (position < pathPattern.size()) {
            char c = pathPattern[position];

            if (c == ' ') {
                regexPattern += "(?:\\+|%20)";// пробел как '+' или '%20'
                position++;
            } else if (c == '{') {
                size_t end = findMatchingBrace(pathPattern, position);
                if (end == std::string::npos) {
                    throw std::runtime_error("Unmatched '{' in path pattern");
                }
                std::string paramContent = pathPattern.substr(position + 1, end - position - 1);
                size_t colon = paramContent.find(':');
                std::string paramName;
                std::string paramRegex;
                if (colon != std::string::npos) {
                    paramName = paramContent.substr(0, colon);
                    paramRegex = paramContent.substr(colon + 1);
                } else {
                    paramName = paramContent;
                    auto it = constraints.find(paramName);
                    paramRegex = (it != constraints.end()) ? it->second->pattern : "[^/]+";
                }

                if (paramName.empty() ||
                    paramName.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != std::string::npos) {
                    throw std::runtime_error("Invalid or empty parameter name: \"" + paramName + "\" in path pattern: \"" + pathPattern + "\"");
                }
                outParamNames.push_back(paramName);
                regexPattern += "(" + paramRegex + ")";
                position = end + 1;
            } else {
                appendEsc(c);// экранируем литералы
                position++;
            }
        }

        try {
            outRegex = std::regex(regexPattern, std::regex::ECMAScript | std::regex::optimize);
        } catch (const std::regex_error &e) {
            throw std::runtime_error("Invalid regex pattern: " + regexPattern + " Error: " + std::string(e.what()));
        }
    }

    Route &RadixRouter::addPlainStringHandler(const std::set<std::string> &method,
                                              const std::string &pathPattern,
                                              std::function<FunctionType> function) {
        return addRoute(method, pathPattern, std::move(function));
    }

    Route &RadixRouter::addHandler(std::string_view method,
                                   const std::string &pathPattern,
                                   std::function<FunctionType> function) {
        std::set<std::string> method_set{std::string(method)};
        return addRoute(method_set, pathPattern, std::move(function));
    }

    Route &RadixRouter::addHandler(const std::set<std::string> &method,
                                   const std::string &pathPattern,
                                   std::function<FunctionType> function,
                                   std::unordered_map<std::string_view, const param_constraint *> &&constraints) {
        return addRoute(method, pathPattern, std::move(function), constraints);
    }

    // ---- Рекурсивный поиск с бэктрекингом ----------------------------------

    bool RadixRouter::matchDFS(RadixNode *node,
                               const std::vector<std::string> &segs,
                               std::size_t idx,
                               Request &req,
                               Route *&out,
                               std::string *last_error) {
        if (idx == segs.size()) {
            if (node->route) {
                const bool req_has_trailing = !req.getURL().empty() && req.getURL().back() == '/';
                if (node->trailing_slash == req_has_trailing) {
                    out = node->route.get();
                    return true;
                }
            }
            return false;
        }

        const std::string &cur = segs[idx];

        // 1) Литерал
        if (auto lit = node->literal.find(cur); lit != node->literal.end()) {
            if (matchDFS(lit->second.get(), segs, idx + 1, req, out, last_error)) {
                return true;
            }
        }

        // 2) Параметры — пробуем все подходящие, БЭКТРЕКИНГ
        std::string local_error;
        for (ParamEdge &edge: node->param) {
            if (edge.regex && std::regex_match(cur, *edge.regex)) {
                // save old value if present
                auto prev_it = req.uri_params.find(edge.name);
                std::optional<std::string> prev_value;
                if (prev_it != req.uri_params.end()) prev_value = prev_it->second;

                req.uri_params[edge.name] = cur;

                if (matchDFS(edge.child.get(), segs, idx + 1, req, out, last_error)) {
                    return true;
                }

                // backtrack
                if (prev_value) req.uri_params[edge.name] = *prev_value;
                else
                    req.uri_params.erase(edge.name);
            } else if (last_error) {
                local_error = edge.constraint ? edge.constraint->description
                                              : ("Invalid value for parameter: " + edge.name);
            }
        }

        // 3) Wildcard — съедаем хвост
        if (node->wildcard) {
            std::string tail;
            for (std::size_t i = idx; i < segs.size(); ++i) {
                tail += '/' + segs[i];
            }
            if (!tail.empty()) tail.erase(0, 1);
            req.uri_params[node->wildcard_name] = tail;
            // Разрешаем матчить дальше: wildcard-узел помечается route только в конце
            if (matchDFS(node->wildcard.get(), segs, segs.size(), req, out, last_error)) {
                return true;
            }
            // backtrack wildcard param
            req.uri_params.erase(node->wildcard_name);
        }

        if (last_error && !local_error.empty()) *last_error = local_error;
        return false;
    }

    bool RadixRouter::matchIter(RadixNode *node,
                                const std::vector<std::string> &segs,
                                Request &req,
                                Route *&out,
                                std::string *last_error) {
        return matchDFS(node, segs, 0, req, out, last_error);
    }

    // ---- Отладочный дамп ----------------------------------------------------

    std::string RadixRouter::dump() const {
        std::ostringstream buf;
        buf << "/\n";
        printNode(root_.get(), buf, "");
        return buf.str();
    }

    void RadixRouter::printNode(const RadixNode *node,
                                std::ostringstream &buf,
                                const std::string &prefix) const {
        size_t total = node->literal.size() + node->param.size() + (node->wildcard ? 1 : 0), idx = 0;

        for (auto &[lbl, ptr]: node->literal) {
            bool last = (++idx == total);
            buf << prefix
                << (last ? "└─" : "├─")
                << lbl
                << (ptr->route ? " [#]" : "")
                << "\n";
            printNode(ptr.get(), buf, prefix + (last ? "   " : "│  "));
        }

        for (auto &pe: node->param) {
            bool last = (++idx == total);
            std::string lbl = ":" + pe.name + (pe.constraint ? "(" + pe.constraint->pattern + ")" : "");
            buf << prefix
                << (last ? "└─" : "├─")
                << lbl
                << (pe.child->route ? " [#]" : "")
                << "\n";
            printNode(pe.child.get(), buf, prefix + (last ? "   " : "│  "));
        }

        if (node->wildcard) {
            bool last = (++idx == total);
            std::string label = "*" + node->wildcard_name;
            buf << prefix
                << (last ? "└─" : "├─")
                << label
                << (node->wildcard->route ? " [#]" : "")
                << "\n";
            printNode(node->wildcard.get(), buf, prefix + (last ? "   " : "│  "));
        }
    }

}// namespace usub::server::protocols::http
