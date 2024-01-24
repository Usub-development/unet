 #include <iostream>
 #include <thread>
 #include "handler/Server.h"
 #include "globals.h"
 #include "parser/Http1Parser.h"

 std::string getFileExtension(const std::string& fileName) {
     size_t dotPos = fileName.find_last_of('.');
     if (dotPos != std::string::npos) {
         return fileName.substr(dotPos + 1);
     }
     return ""; // Return empty string if no extension found
 }

 long getFileSize(const std::string& fileName) {
     std::ifstream file(fileName, std::ifstream::ate | std::ifstream::binary);
     if (!file.is_open()) {
         std::cerr << "Could not open file: " << fileName << std::endl;
         return -1; // Return -1 or throw an exception if file can't be opened
     }
     return file.tellg();
 }

 void serverThreadFunction() {
     unit::server::handler::Server server("redirectConfig.toml");
     server.handle(unit::server::request::GET, R"(/)",
               [&](unit::server::data::HttpRequest&request, unit::server::data::HttpResponse&response) {
 	      	  response.setStatus(301);
 		  response.addHeader("Location", "https://example.com");
               });
    server.start();
 }

 void stopAllEventBases() {
 	std::lock_guard<std::mutex> lock(eventBasesMutex);
 	    for (auto base : eventBases) {
 	         event_base_loopbreak(base);
 	    }
 }

 void signalHandler(int signum) {
     stopAllEventBases();
 }

 int main()
 {
      std::signal(SIGINT, signalHandler);
      std::signal(SIGPIPE, SIG_IGN);
      std::signal(SIGTERM, signalHandler);

      unit::server::handler::Server server("config.toml");
      server.handle(unit::server::request::GET, R"(/)",
                [&](unit::server::data::HttpRequest&request, unit::server::data::HttpResponse&response) {
                    response.writeFile("index.html");
                    response.addHeader((char *) ":status", (char *) "200");
                    response.addHeader((char *) "Content-Type", (char *) "text/html; charset=utf-8");
                });
      server.handle(unit::server::request::GET, R"(^\/.*\.[a-zA-Z0-9]+$)",
                    [&](unit::server::data::HttpRequest&request, unit::server::data::HttpResponse&response) {
                        std::string path = server.config->getKey("doc_root").as_string()->get() + request.headers.at(":path");
                        long size = getFileSize(path);
                        std::string extension = "application/";
                        extension.append(getFileExtension(path) == "js" ? "javascript" : "css");
                        char buffer[20];
                        snprintf(buffer, sizeof(buffer), "%ld", size);
                        response.writeFile(path);
                        response.addHeader(":status", "200");
                        path.clear();
                    });
      std::thread serverThread(serverThreadFunction);
      serverThread.detach();
      server.start();
     return 0;
 }